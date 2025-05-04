#ifndef PCB_H
#define PCB_H

#include <comp421/hardware.h>
#include <comp421/loadinfo.h>
#include <comp421/yalnix.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Process state definitions */
#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define TERMINATED 3

/* Physical memory management */
#define MAX_PHYS_PAGES (VMEM_SIZE / PAGESIZE)  // Based on total virtual memory size

/* Process scheduling */
#define TIME_SLICE 2

/* Exit node structure */
typedef struct exitNode {
  int pid;
  int exit_status;
  struct exitNode *next;
} exitNode;

typedef struct childNode {
  int pid;
  struct childNode *next;
  struct ProcessControlBlock *pcb;
} childNode;

/* Process Control Block structure */
typedef struct ProcessControlBlock
{
    int pid;                                // Process ID
    struct ProcessControlBlock *next;       // Next PCB in queue
    struct ProcessControlBlock *prev;       // Previous PCB in queue
    struct ProcessControlBlock *parent;     // Parent process
    int children_num;                       // Number of children (running and exited, will decrement after parent calls wait)

    SavedContext ctx;                  // Directly embedded context instead of pointer
    struct pte *page_table;            // Process page table
    
    void *stack_brk;            // User stack boundary
    void *heap_brk;             // User heap boundary
    
    int status;                 // Process status (READY, RUNNING, etc.)

    long time_slice;             // Time slice counter for round-robin scheduling, each process runs 2 clock ticks
    long delay_ticks;            // Block until delay_ticks clock interrupts

    struct exitNode *exitQ;     // Exited child queue
    struct childNode *childQ;   // Child process queue
    
    /* terminal operation's variables */
    char read_buf[TERMINAL_MAX_LINE];       // 用於終端讀取的緩衝區
    int read_len;                           // 終端讀取的長度
    int read_result;                        // 終端讀取的結果

} ProcessControlBlock;

/* PCB queue structure */
typedef struct PCBqueue {
    ProcessControlBlock *head;         // Queue head
    ProcessControlBlock *tail;         // Queue tail
} PCBqueue;

// Terminal buffer structure
typedef struct {
    char data[TERMINAL_MAX_LINE];   // Buffer for terminal data
    int size;                       // Number of item in the buffer
} TerminalBuffer;

typedef struct ReceivedLine {
  char *data;
  char *original_data;
  int size;
  struct ReceivedLine *next;
} ReceivedLine;

// Terminal buffers and queues 
extern TerminalBuffer terminal_buffers[NUM_TERMINALS];
extern ReceivedLine *firstReceivedLine[NUM_TERMINALS];
extern PCBqueue *tty_read_blocked_queue[NUM_TERMINALS];
extern PCBqueue *tty_write_blocked_queue[NUM_TERMINALS];
extern int terminalTransmitBusy[NUM_TERMINALS];

/* 
 * Each pageTableRecord is used to ensure that page tables were physically contiguous in memory 
 * Since the size of the page table is PAGESIZE/2, a physical page can carry two page tables
 * The pageTableRecord is used to keep track of the free half physical pages to use when allocating memory for page tables
 * We store the page table for user processes in the top of region 1
*/
struct pageTableRecord{
  void *region1_base;             // The page address for this record in region 1
  int pfn;                        // The physical frame number for this page
  int isTopFull;                  // To check if the first half is used for a page table for a process
  int isBottomFull;               // To check if the second half is used for a page table for a process
  struct pageTableRecord *next;   // Pointer to the next pageTableRecord
};
extern struct pageTableRecord *firstPageTableRecord;

/* Global variables */
extern void *kernel_brk;                    // Kernel break pointer
extern int *free_frame_map;                 // Physical frame allocation map (0=free, 1=used)
extern int num_physical_pages;              // Total number of physical pages
extern struct pte *kernelpt;                // Kernel page table
extern int isVMEnabled;                     // Flag indicating if VM is enabled
extern void (*trap_vector[TRAP_VECTOR_SIZE])(ExceptionInfo *); // Trap handler vector
extern int next_pid;                        // Next process ID
extern long clock_ticks;                     // Clock ticks counter
extern void* void_page;                   // A void page for page copy

/* Process pointers */
extern ProcessControlBlock *activeProcess;  // Currently running process
extern ProcessControlBlock *init;           // Initial process
extern ProcessControlBlock *idle;           // Idle process

/* Process scheduling queues */
extern PCBqueue *readyQueue;            // Queue for processes in READY state
extern PCBqueue *blockedQueue;            // Queue for processes in BLOCKED state

/* Context switching functions */
extern SavedContext *generalSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *forkSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *exitSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *waitSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *ttySwitch(SavedContext *ctxp, void *p1, void *p2);
extern void changeReg0(void* destPT_virtual);

/* Memory management functions */
extern void InitializeMemory(unsigned int pmem_size);
extern int AllocatePage(void);
extern void FreePage(int pfn);
extern int CountFreePhysicalPages(void);
extern int CountAllocatedUserPages(void);
extern void initFirstPTRecord(void);
extern struct pte *createPageTable(void);
extern void InitializeKernelPageTable(void);
extern void EnableVirtualMemory(void);
extern int ExpandStack(ExceptionInfo *info);
extern void FreeRegion0Pages(struct pte *userpt);
extern int SetBrk(void *addr, ProcessControlBlock *pcb);
extern void CopyPage(struct pte *src_pt, struct pte *dest_pt);
extern void DeletePageTable(struct pte *pt);

/* Queue management functions */
extern void AddToReadyQueue(ProcessControlBlock *pcb);
extern void AddToBlockedQueue(ProcessControlBlock *pcb);
extern ProcessControlBlock *RemoveFromQueue(PCBqueue *q);
extern int RemoveSpecificPCB(PCBqueue *q, ProcessControlBlock *pcb);
extern int IsQueueEmpty(PCBqueue *q);
extern ProcessControlBlock *GetNextReadyProcess();

/* Process management functions */
extern ProcessControlBlock *CreateNewProcess(int pid, ProcessControlBlock* parentPCB);
extern void DestroyProcess(ProcessControlBlock *pcb);
extern exitNode *popExitQ(ProcessControlBlock *pcb);

/* Kernel Call functions */
extern int KernelFork(void);
extern int KernelExec(ExceptionInfo *info);
extern void KernelExit(int status);
extern int KernelWait(int *status_ptr);
extern int KernelGetPid(void);
extern int KernelBrk(void *addr);
extern int KernelDelay(int clock_ticks);
extern int KernelTtyRead(int tty_id, void *buf, int len);
extern int KernelTtyWrite(int tty_id, void *buf, int len);

/* Trap Handler */
extern void TrapKernel(ExceptionInfo *info);
extern void TrapClock(ExceptionInfo *info);
extern void TrapIllegal(ExceptionInfo *info);
extern void TrapMemory(ExceptionInfo *info);
extern void TrapMath(ExceptionInfo *info);
extern void TrapTtyReceive(ExceptionInfo *info);
extern void TrapTtyTransmit(ExceptionInfo *info);

/* Kernel functions */
extern void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args);
extern int SetKernelBrk(void *addr);

extern int LoadProgram(char *name, char **args, ExceptionInfo *info, ProcessControlBlock *pcb);

/* Terminal functions */
extern void InitializeTerminals();
extern void AddToQueue(PCBqueue *q, ProcessControlBlock *pcb);
#endif // PCB_H