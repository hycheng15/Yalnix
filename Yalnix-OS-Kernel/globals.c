#include <comp421/hardware.h>
#include "pcb.h"

// Global variables 
void *kernel_brk = NULL;
int *free_frame_map = NULL;
int num_physical_pages = 0;
struct pte *kernelpt = NULL;
int isVMEnabled = 0;
void (*trap_vector[TRAP_VECTOR_SIZE])(ExceptionInfo *);
int next_pid = 2;   // Since pid = 0 is idle process, pid = 1 is init process
long clock_ticks = 0;

// Processs scheduling and control
ProcessControlBlock *activeProcess = NULL;
ProcessControlBlock *init = NULL;
ProcessControlBlock *idle = NULL;

PCBqueue *readyQueue;                               // Queue for processes in READY state
PCBqueue *blockedQueue;                             // Queue for process in BLOCKED state
PCBqueue *tty_read_blocked_queue[NUM_TERMINALS];    // Queue for processes in BLOCKED state waiting for TTY input
PCBqueue *tty_write_blocked_queue[NUM_TERMINALS];   // Queue for processes in BLOCKED state waiting for TTY output
int terminalTransmitBusy[NUM_TERMINALS] = {0};      // 0: not busy, 1: busy
ReceivedLine *firstReceivedLine[NUM_TERMINALS];     // Buffer for received lines