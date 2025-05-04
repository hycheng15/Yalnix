/*
* Purpose: This is the entry point for your Yalnix kernel. 
* It initializes memory, sets up system calls, and starts the first process.
*/

// #include "include/hardware.h"
// #include "include/yalnix.h"
// #include "include/loadinfo.h"
#include <comp421/hardware.h>
#include <comp421/loadinfo.h>
#include <comp421/yalnix.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pcb.h"

// Kernel page table
extern struct pte *kernelpt;

// Memory management
extern void *void_page;

// Process variables
extern ProcessControlBlock *activeProcess;
extern ProcessControlBlock *init;
extern ProcessControlBlock *idle;

// Kernel heap top's address
extern void *kernel_brk;

// Vector table for trap handlers
extern void (*trap_vector[TRAP_VECTOR_SIZE])(ExceptionInfo *);

// Forward declarations for trap handlers
extern void TrapKernel(ExceptionInfo *);
extern void TrapClock(ExceptionInfo *);
extern void TrapIllegal(ExceptionInfo *);
extern void TrapMemory(ExceptionInfo *);
extern void TrapMath(ExceptionInfo *);
extern void TrapTtyReceive(ExceptionInfo *);
extern void TrapTtyTransmit(ExceptionInfo *);

// Forward declarations for system calls
extern int LoadProgram(char *name, char **args, ExceptionInfo *info, ProcessControlBlock *pcb);
extern void InitializeKernelPageTable(void);
extern void EnableVirtualMemory(void);
extern void InitializeMemory(unsigned int pmem_size);
extern SavedContext *generalSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *forkSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *exitSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *waitSwitch(SavedContext *ctxp, void *p1, void *p2);
extern SavedContext *ttySwitch(SavedContext *ctxp, void *p1, void *p2);
extern int SetKernelBrk(void *addr);
extern ProcessControlBlock *CreateNewProcess(int pid, ProcessControlBlock* parentPCB);
extern void InitializeTrapVector(void);
void changeReg0(void *destPT_virtual);

/* Forward declarations for terminal */
extern void InitializeTerminals();

int isInit = 1;
/**
 * General context switch function for process switching
 * Adds the current process back to the ready queue and set it's status to READY
 * Removes the next process from the ready queue and set it's status to RUNNING
 * @param ctxp Context pointer
 * @param p1 Current process
 * @param p2 Next process
 * @return Saved context of next process
 */
SavedContext *generalSwitch(SavedContext *ctxp, void *p1, void *p2) {
    ProcessControlBlock *current = (ProcessControlBlock *)p1;
    ProcessControlBlock *next = (ProcessControlBlock *)p2;

    TracePrintf(3, "generalSwitch: Switch from process %d to process %d\n", current->pid, next->pid);
    if (next == NULL) {
        TracePrintf(0, "generalSwitch: ERROR - next process is NULL. Halting...\n");
        printf("ERROR: next process is NULL. Halting...\n");
        Halt();
    }
    
    // Switch to next process' page table and set active process to next
    TracePrintf(5, "generalSwitch: Next page table is at %p\n", next->page_table);
    changeReg0(next->page_table);
    activeProcess = next;
    
    // Add the process to readyQueue only if it's not idle and NULL
    if (current != NULL && current->pid != 0) {
        AddToReadyQueue(current);
    }
    
    next->status = RUNNING;
    if (next->pid != 0) {
        RemoveSpecificPCB(readyQueue, next);
    }
    
    TracePrintf(5, "generalSwitch: Switched from process %d to process %d\n", current ? current->pid : -1, next->pid);
    
    return &next->ctx;
}

/**
 * Context switch function specifically for the Fork system call
 * Adds the current process back to the ready queue and set it's status to READY
 * Removes the next process from the ready queue and set it's status to RUNNING
 * @param ctxp Context pointer
 * @param p1 Parent process
 * @param p2 Child process
 * @return Saved context of child process
 */
SavedContext *forkSwitch(SavedContext *ctxp, void *p1, void *p2) {
    ProcessControlBlock *parent = (ProcessControlBlock *)p1;
    ProcessControlBlock *child = (ProcessControlBlock *)p2;
    
    TracePrintf(3, "forkSwitch: Switch from %d to %d\n", parent->pid, child->pid);

    TracePrintf(5, "forkSwitch: Copy parent's region 0 pages to child's region 0 pages\n");
    CopyPage(parent->page_table, child->page_table);

    // Switch to child's page table and set active process to child
    changeReg0(child->page_table);
    activeProcess = child;

    // Change the status of both process and add them to queue respectively
    if (parent != idle) {
        AddToReadyQueue(parent);
    }
    child->status = RUNNING;
    RemoveSpecificPCB(readyQueue, child);
    
    TracePrintf(5, "forkSwitch: Fork completed: Created process %d from parent %d\n", 
                child->pid, parent->pid);
    
    // Return parent's context so that the child will have a copy
    return &parent->ctx;
}

/**
 * Context switch function specifically for the Exit system call
 * Set current process' status to TERMINATED
 * Removes the next process from the ready queue and set it's status to RUNNING
 * @param ctxp Context pointer
 * @param p1 Current process
 * @param p2 Next process
 * @return Saved context of next process
 */
SavedContext *exitSwitch(SavedContext *ctxp, void *p1, void *p2) {    
    ProcessControlBlock *current = (ProcessControlBlock *)p1;
    ProcessControlBlock *next = (ProcessControlBlock *)p2;
    TracePrintf(3, "exitSwitch: process %d is exiting and switching to process %d\n", 
        current->pid, next->pid);
    
    // Set current (exiting process) status to TERMINATED
    current->status = TERMINATED;
    
    // Free all resources used by the activeProcess
    DestroyProcess(activeProcess);

    // Switch to next process' page table and set active process to next
    changeReg0(next->page_table);
    activeProcess = next;

    // Change the status of both process and add them to queue respectively
    next->status = RUNNING;
    if (next->pid != 0) {
        RemoveSpecificPCB(readyQueue, next);
    }

    TracePrintf(5, "exitSwitch: Switched to process %d\n", next->pid);
    
    // Return context of next process
    return &next->ctx;
}

/**
 * Context switch function specifically for the Wait system call
 * Adds the current process to the blocked queue and set it's status to BLOCKED
 * Removes the next process from the ready queue and set it's status to RUNNING
 * @param ctxp Context pointer
 * @param p1 Current process
 * @param p2 Next process
 * @return Saved context of next process
 */
SavedContext *waitSwitch(SavedContext *ctxp, void *p1, void *p2) {
    ProcessControlBlock *current = (ProcessControlBlock *)p1;
    ProcessControlBlock *next = (ProcessControlBlock *)p2;

    current->status = BLOCKED;

    TracePrintf(3, "waitSwitch: process %d is blocked and switching to process %d\n", 
        current->pid, next->pid);

    // Switch to next process' page table and set active process to next
    changeReg0(next->page_table);
    activeProcess = next;
    
    // Change the status of both process and add them to queue respectively
    if (current != idle) {
        AddToBlockedQueue(current);
    }
    next->status = RUNNING;
    if (next->pid != 0) {
        RemoveSpecificPCB(readyQueue, next);
    }

    TracePrintf(5, "waitSwitch: Switched from process %d to process %d\n", current->pid, next->pid);

    // Return context of next process
    return &next->ctx;
}

/**
 * Context switch function specifically for the TTY related system call
 * p1 should've been set to BLOCKED and add to respective queue (for read/write)
 * Removes the next process from the ready queue and set it's status to RUNNING
 * @param ctxp Context pointer
 * @param p1 Current process
 * @param p2 Next process
 * @return Saved context of next process
 */
SavedContext *ttySwitch(SavedContext *ctxp, void *p1, void *p2) {
    ProcessControlBlock *current = (ProcessControlBlock *)p1;
    ProcessControlBlock *next = (ProcessControlBlock *)p2;

    TracePrintf(3, "ttySwitch: process %d is blocked and switching to process %d\n", 
        current->pid, next->pid);

    // Switch to next process' page table and set active process to next
    changeReg0(next->page_table);
    activeProcess = next;
    
    // Change the status of next process and remove it from readyQ
    next->status = RUNNING;
    if (next->pid != 0) {
        RemoveSpecificPCB(readyQueue, next);
    }

    TracePrintf(5, "ttySwitch: Switched from process %d to process %d\n", current->pid, next->pid);

    // Return context of next process
    return &next->ctx;
}

/**
 * Change REG_PTR0 to the page table virtual address specified
 * Will also flush the TLB for region 0
 * @param destPT Target page table virtual address
 */
void changeReg0(void* destPT_virtual) {
    // Change the virtual address to physical address first
    void* destPT_virtual_base = (void*)DOWN_TO_PAGE(destPT_virtual);
    void *destPT_physical_base;
    void *destPT_physical;
    int pfn;

    // If the address is in region 0, use the user page table
    if (destPT_virtual_base < (void *)VMEM_1_BASE) {
        TracePrintf(6, "changeReg0: addr is in region 0, %p\n", destPT_virtual_base);
        pfn = activeProcess->page_table[((long)destPT_virtual_base - VMEM_0_BASE) / PAGESIZE].pfn;
        destPT_physical_base = (void *)((long)(pfn * PAGESIZE));
    } 
    // If the address is in region 1, use the kernel page table
    else {
        TracePrintf(6, "changeReg0: addr is in region 1, %p\n", destPT_virtual_base);
        pfn = kernelpt[((long)destPT_virtual_base - VMEM_1_BASE) / PAGESIZE].pfn;
        destPT_physical_base = (void *)((long)(pfn * PAGESIZE));
    }
    destPT_physical = (void*)((long)destPT_physical_base + ((long)destPT_virtual & PAGEOFFSET));

    TracePrintf(6, "changeReg0: Changing REG_PTR0 to physical addr %p\n", destPT_physical);
	WriteRegister(REG_PTR0, (RCS421RegVal)destPT_physical);
	WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
}

/**
 * Sets the kernel break address
 * @param addr New break address
 * @return 0 on success, -1 on failure
 */
int SetKernelBrk(void *addr) {
    TracePrintf(4, "SetKernelBrk: Requested address = %p\n", addr);
    TracePrintf(5, "SetKernelBrk: Current kernel_brk = %p\n", kernel_brk);
    
    unsigned long heap_top = (unsigned long)addr;
    TracePrintf(5, "SetKernelBrk: Requested kernel heap top = %p\n", (void *)heap_top);
    
    unsigned long aligned_heap_top = UP_TO_PAGE(heap_top);
    TracePrintf(5, "SetKernelBrk: Aligned kernel heap top = %p\n", (void *)aligned_heap_top);
    
    // Check if exceeding boundary
    if (aligned_heap_top >= (unsigned long)VMEM_1_LIMIT) {
        TracePrintf(0, "SetKernelBrk: ERROR - kernel heap top would exceed kernel boundary\n");
        TracePrintf(0, "SetKernelBrk: aligned_heap_top (%p) >= VMEM_1_LIMIT (%p)\n", 
                   (void *)aligned_heap_top, (void *)VMEM_1_LIMIT);
        return -1;
    }
    
    // If virtual memory is enabled, need to allocate physical pages
    if (isVMEnabled) {
        TracePrintf(5, "SetKernelBrk: Virtual memory is enabled\n");
        
        // If heap boundary does not change
        if (kernel_brk != NULL && aligned_heap_top == (unsigned long)kernel_brk) {
            TracePrintf(5, "SetKernelBrk: No change in heap boundary\n");
        }
        // If heap boundary expands
        else if (kernel_brk != NULL && aligned_heap_top > (unsigned long)kernel_brk) {
            int num_needed_pages = (aligned_heap_top - (unsigned long)kernel_brk) / PAGESIZE;
            int free_pages = CountFreePhysicalPages();
            
            TracePrintf(5, "SetKernelBrk: Heap expansion: current=%p, new=%p, needed pages=%d, available pages=%d\n", 
                       kernel_brk, (void *)aligned_heap_top, num_needed_pages, free_pages);
            
            if (num_needed_pages > free_pages) {
                TracePrintf(0, "SetKernelBrk: ERROR - Not enough physical pages available\n");
                return -1;
            }
            
            // Set up page table entries for each new page
            for (unsigned long addr = UP_TO_PAGE((unsigned long)kernel_brk); addr < aligned_heap_top; addr += PAGESIZE) {
                unsigned int vpn = (addr - VMEM_1_BASE) >> PAGESHIFT;
                TracePrintf(5, "SetKernelBrk: Allocating page for address %p, VPN = %u\n", (void *)addr, vpn);
                
                // Allocate physical page
                int pfn = AllocatePage();
                if (pfn < 0) {
                    TracePrintf(0, "SetKernelBrk: ERROR - Could not allocate physical page\n");
                    return -1;
                }
                
                TracePrintf(5, "SetKernelBrk: Allocated page PFN = %d\n", pfn);
                
                // Update kernel page table entry
                kernelpt[vpn].valid = 1;
                kernelpt[vpn].uprot = PROT_NONE;
                kernelpt[vpn].kprot = PROT_READ | PROT_WRITE;
                kernelpt[vpn].pfn = pfn;

                TracePrintf(5, "SetKernelBrk: Updated kernel page table entry for VPN %u\n", vpn);
                
                // Clear page content
                memset((void *)addr, 0, PAGESIZE);

                TracePrintf(5, "SetKernelBrk: Cleared page content for address %p\n", (void *)addr);
            }
            TracePrintf(5, "SetKernelBrk: Updating kernel_brk from %p to %p\n", kernel_brk, (void *)aligned_heap_top);
            kernel_brk = (void *)aligned_heap_top;
        } 
        // If heap boundary shrinks
        else if (kernel_brk != NULL && aligned_heap_top < (unsigned long)kernel_brk) {
            TracePrintf(5, "SetKernelBrk: Heap shrinking: current=%p, new=%p\n", 
                       kernel_brk, (void *)aligned_heap_top);
            
            // Free unused pages
            for (unsigned long addr = aligned_heap_top; addr < (unsigned long)UP_TO_PAGE(kernel_brk); addr += PAGESIZE) {
                unsigned int vpn = addr >> PAGESHIFT;
                TracePrintf(5, "SetKernelBrk: Freeing page for address %p, VPN = %u\n", (void *)addr, vpn);
                
                if (kernelpt[vpn].valid) {
                    FreePage(kernelpt[vpn].pfn);
                    kernelpt[vpn].valid = 0;
                    TracePrintf(5, "SetKernelBrk: Freed PFN = %d\n", kernelpt[vpn].pfn);
                }
            }
            TracePrintf(5, "SetKernelBrk: Updating kernel_brk from %p to %p\n", kernel_brk, (void *)aligned_heap_top);
            kernel_brk = (void *)aligned_heap_top;
        }
    }
    // If virtual memory is not enabled, only update kernel_brk 
    else {
        TracePrintf(5, "SetKernelBrk: Virtual memory not enabled, only updating kernel_brk\n");
        TracePrintf(5, "SetKernelBrk: Updating kernel_brk from %p to %p\n", kernel_brk, (void *)aligned_heap_top);
        kernel_brk = (void *)aligned_heap_top;
    }
    
    TracePrintf(1, "SetKernelBrk: Successfully set kernel break to %p\n", (void *)aligned_heap_top);
    return 0;
}

/**
 * Creates and initializes a new process
 * @param pid New process' pid
 * @param parentPCB Pointer to the parent process' PCB
 * @return Pointer to the new process PCB
 */
ProcessControlBlock *CreateNewProcess(int pid, ProcessControlBlock* parentPCB) {
    ProcessControlBlock *pcb = (ProcessControlBlock *)malloc(sizeof(ProcessControlBlock));
    if (pcb == NULL) {
        TracePrintf(0, "CreateNewProcess: ERROR - failed to allocate PCB\n");
        return NULL;
    }

    TracePrintf(1, "CreateNewProcess: Create new process with PID %d\n", pid);
    
    // Initialize process control block
    pcb->pid = pid;
    pcb->parent = parentPCB;
    pcb->children_num = 0;
    pcb->next = NULL;
    pcb->prev = NULL;
    pcb->status = READY;
    pcb->time_slice = TIME_SLICE;
    pcb->stack_brk = (void *)USER_STACK_LIMIT;
    pcb->heap_brk = (void *)MEM_INVALID_SIZE;
    pcb->exitQ = NULL;
    pcb->childQ = NULL;

    // Create page table for the idle process
    pcb->page_table = createPageTable();
    TracePrintf(5, "CreateNewProcess: New process' pt is at %p\n", (void *)pcb->page_table);
    
    // Initialize page table
    if (pcb->pid == 0) {
        for (int i = 0; i < PAGE_TABLE_LEN; i++) {
            if (i >= KERNEL_STACK_BASE >> PAGESHIFT) {
                // kernel stack pages
                pcb->page_table[i].valid = 1;
                pcb->page_table[i].kprot = PROT_READ | PROT_WRITE;
                pcb->page_table[i].uprot = PROT_NONE;
                pcb->page_table[i].pfn = i;
                TracePrintf(6, "CreateNewProcess: Kernel stack page vpn %d (addr %p), pfn %d (addr %p)\n", 
                    i, (i * PAGESIZE), pcb->page_table[i].pfn, (pcb->page_table[i].pfn * PAGESIZE));
                TracePrintf(6, "CreateNewProcess: pfn %d's free_frame_map is %d\n", 
                    pcb->page_table[i].pfn, free_frame_map[pcb->page_table[i].pfn]);
            } 
            else {
                // non kernel stack pages
                pcb->page_table[i].valid = 0;
                pcb->page_table[i].kprot = PROT_NONE;
                pcb->page_table[i].uprot = PROT_READ | PROT_WRITE | PROT_EXEC;
            }            
        }
    } 
    else {
        for (int i = 0; i < PAGE_TABLE_LEN; i++) {
            if (i < KERNEL_STACK_BASE >> PAGESHIFT) {
                // non kernel stack pages
                pcb->page_table[i].valid = 0;
                pcb->page_table[i].kprot = PROT_READ | PROT_WRITE;
                pcb->page_table[i].uprot = PROT_READ | PROT_WRITE | PROT_EXEC;
            } 
            else {
                // kernel stack pages
                pcb->page_table[i].valid = 1;
                pcb->page_table[i].kprot = PROT_READ | PROT_WRITE;
                pcb->page_table[i].uprot = PROT_NONE;
                pcb->page_table[i].pfn = AllocatePage();
            }
        }
        pcb->heap_brk = parentPCB->heap_brk;
        pcb->stack_brk = parentPCB->stack_brk;
    }
    
    TracePrintf(5, "CreateNewProcess: New process created with PID %d\n", pcb->pid);
    return pcb;
}

/**
 * Initialize trap vector table
 */
void InitializeTrapVector() {
    // Clear the entire trap vector
    for (int i = 0; i < TRAP_VECTOR_SIZE; i++) {
        trap_vector[i] = NULL;
    }
    
    // Set up trap handlers
    trap_vector[TRAP_KERNEL] = TrapKernel;
    trap_vector[TRAP_CLOCK] = TrapClock;
    trap_vector[TRAP_ILLEGAL] = TrapIllegal;
    trap_vector[TRAP_MEMORY] = TrapMemory;
    trap_vector[TRAP_MATH] = TrapMath;
    trap_vector[TRAP_TTY_RECEIVE] = TrapTtyReceive;
    trap_vector[TRAP_TTY_TRANSMIT] = TrapTtyTransmit;
    
    // Set the vector base register
    WriteRegister(REG_VECTOR_BASE, (RCS421RegVal)trap_vector);
    
    TracePrintf(0, "InitializeTrapVector: Trap vector initialized\n");
}

/**
 * Initialize terminal buffers and queues
 * @return 0 on success, -1 on failure
 */
void InitializeTerminals() {
    for (int i = 0; i < NUM_TERMINALS; i++) {
        firstReceivedLine[i] = NULL;
        
        tty_read_blocked_queue[i] = (PCBqueue *)malloc(sizeof(PCBqueue));
        if (tty_read_blocked_queue[i] == NULL) {
            TracePrintf(0, "InitializeTerminals: ERROR - Failed to create read queue for terminal %d\n", i);
            Halt();
        }
        tty_read_blocked_queue[i]->head = NULL;
        tty_read_blocked_queue[i]->tail = NULL;
        
        tty_write_blocked_queue[i] = (PCBqueue *)malloc(sizeof(PCBqueue));
        if (tty_write_blocked_queue[i] == NULL) {
            TracePrintf(0, "InitializeTerminals: ERROR - Failed to create write queue for terminal %d\n", i);
            Halt();
        }
        tty_write_blocked_queue[i]->head = NULL;
        tty_write_blocked_queue[i]->tail = NULL;
    }
    
    TracePrintf(0, "InitializeTerminals: Terminal buffers and queues initialized\n");
}

/**
 * Entry point for the Yalnix kernel
 * @param info Initial exception info
 * @param pmem_size Total physical memory size
 * @param orig_brk Original kernel break
 * @param cmd_args Command line arguments
 */
void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args) {

    TracePrintf(0, "KernelStart: Starting Yalnix kernel\n");
    TracePrintf(5, "KernelStart: Physical memory size: %u bytes\n", pmem_size);
    TracePrintf(5, "KernelStart: Original break address: %p\n", orig_brk);
    TracePrintf(5, "KernelStart: KERNEL_STACK_BASE: %p\n", (void *)KERNEL_STACK_BASE);
    
    // Step 1: Set the kernel break
    // Step 2: Initialize memory management
    // Step 3: Initialize page tables
    // Step 4: Initialize trap vector table
    // Step 5: Enable virtual memory
    // Step 6: Create idle process
    // Step 7: Create init process
    // Step 8: Set active process to init and start execution

    // Initialize readyQueue and blockedQueue
    readyQueue = malloc(sizeof(PCBqueue));
    readyQueue->head = NULL;
    readyQueue->tail = NULL;

    blockedQueue = malloc(sizeof(PCBqueue));
    blockedQueue->head = NULL;
    blockedQueue->tail = NULL;

    SetKernelBrk(orig_brk);

    InitializeMemory(pmem_size);

    InitializeTrapVector();
    
    InitializeKernelPageTable();

    initFirstPTRecord();

    // Create the idle process
    TracePrintf(5, "KernelStart: Creating idle process\n");
    idle = CreateNewProcess(0, NULL);
    if (idle == NULL) {
        TracePrintf(0, "KernelStart: ERROR - failed to create idle process. Halting...\n");
        printf("Error: failed to create idle process. Halting...\n");
        Halt();
    }
    
    // Set the base addresses for Region 0 and Region 1 page tables
    TracePrintf(5, "KernelStart: Write idle pt to register\n");
    WriteRegister(REG_PTR0, (RCS421RegVal)idle->page_table);
    TracePrintf(5, "KernelStart: Write kernelpt to register\n");
    WriteRegister(REG_PTR1, (RCS421RegVal)kernelpt);

    TracePrintf(5, "Enable VM\n");
    EnableVirtualMemory();
    
    InitializeTerminals();

    TracePrintf(5, "activeProcess = idle\n");
    activeProcess = idle;
    char *loadargs[2];
    loadargs[0] = "idle";
    loadargs[1] = NULL;  

    void_page = malloc(PAGESIZE);
    
    TracePrintf(5, "kernelStart: Starting to load idle program\n");
    if (LoadProgram("idle", loadargs, info, activeProcess) < 0){
        TracePrintf(0, "kernelStart: Error - failed to load idle. Halting...\n");
        printf("ERROR: failed to load idle. Halting...\n");
        Halt();
    }
    TracePrintf(5, "kernelStart: Loaded idle program\n");
    TracePrintf(5, "kernelStart: idle program loaded, now switching to idle process\n");
    changeReg0(idle->page_table);
    activeProcess = idle;
    TracePrintf(5, "kernelStart: Switched to idle process\n");

    // Create the init process
    TracePrintf(5, "KernelStart: Creating init process\n");
    init = CreateNewProcess(1, idle);
    if (init == NULL) {
        TracePrintf(0, "KernelStart: ERROR - failed to create init process. Halting...\n");
        printf("ERROR: failed to create init. Halting...\n");
        Halt();
    }

    // Context switch to the init process
    TracePrintf(5, "KernelStart: Initialization complete, starting init process\n");
    AddToReadyQueue(init);
    ContextSwitch(forkSwitch, &idle->ctx, idle, init);
    TracePrintf(0, "KernelStart: Context switch to init process complete\n");
    if (isInit == 1){
        isInit = 0;
        if (cmd_args[0] != NULL) {
            if (LoadProgram(cmd_args[0], cmd_args, info, init) < 0){
            	TracePrintf(0, "kernelStart: Failed to load %s. Halting...\n", cmd_args[0]);
                printf("ERROR: Failed to load %s. Halting...\n", cmd_args[0]);
            	Halt();
            }
        }
        else {
            loadargs[0] = "init";
            loadargs[1] = NULL;
            if (LoadProgram(loadargs[0], loadargs, info, init) < 0) {
            	TracePrintf(0, "kernelStart: Failed to load %s. Halting...\n", loadargs[0]);
            	printf("ERROR: Failed to load %s. Halting...\n", loadargs[0]);
            	Halt();
            }
        }
        TracePrintf(5, "kernelStart: Loaded init program\n");
    }
    
    return;
}