/*
* Purpose: Implements system calls for process control, memory management, and I/O.
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
#include <unistd.h>
#include "pcb.h"

/**
 * Creates a new process by copying the parent process's memory
 * @return PID of child to parent, 0 to child, ERROR on failure
 */
int KernelFork(void) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelFork: ERROR - no active process\n");
        return ERROR;
    }

    // Check if there is enough physical pages for new process
    if (CountAllocatedUserPages() + KERNEL_STACK_PAGES > CountFreePhysicalPages()) {
        TracePrintf(0, "KernelFork: ERROR - not enough physical page to fork, \n");
        return ERROR;
    }

    // Create new PCB
    ProcessControlBlock *child_process = CreateNewProcess(next_pid++, activeProcess);

    // Increment the child process count for calling parent process
    activeProcess->children_num++;

    // Add the child process to the parent's childQ
    struct childNode *newChild = malloc(sizeof(struct childNode));
    newChild->pid = child_process->pid;
    newChild->next = NULL;
    newChild->pcb = child_process;

    struct childNode *current_child = activeProcess->childQ;
    if (current_child) {
        while (current_child->next) {    // Traverse to the end of the child queue
            current_child = current_child->next;
        }
        current_child->next = newChild;
    } 
    else {
        activeProcess->childQ = newChild;
    }
    
    // Record parent's pid 
    int parent_pid = activeProcess->pid;

    // Context switch to new process and back
    TracePrintf(3, "KernelFork: Context switching from parent pid %d to child pid %d\n", activeProcess->pid, child_process->pid);
    ContextSwitch(forkSwitch, &(activeProcess->ctx), activeProcess, child_process);

    // Code after context switch executes in both parent and child processes
    // Check if current process is the parent
    if (activeProcess->pid == parent_pid) {
        // Parent process
        return child_process->pid;
    } else {
        // Child process
        return 0;
    }
}

/**
 * Loads a new program into the process's memory
 * @param filename Path to executable file
 * @param argvec Command line arguments
 * @return 0 on success, ERROR on failure
 */
int KernelExec(ExceptionInfo *info) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelExec: ERROR - no active process\n");
        return ERROR;
    }
    
    char *filename = (char *)info->regs[1];
    char **argvec = (char **)info->regs[2];

    TracePrintf(2, "KernelExec called by PID %d for program %s\n", activeProcess->pid, filename);
    
    // Use LoadProgram to load the new program
    int status = LoadProgram(filename, argvec, info, activeProcess);
    if (status == -1) {
        TracePrintf(0, "KernelExec: Error - LoadProgram failed with status %d\n", status);
        return ERROR;
    }
    if (status == -2) {
        TracePrintf(0, "KernelExec: Error - LoadProgram failed with non-recoverable error, exiting process\n");
        KernelExit(ERROR);
    }
    return 0;
}

/**
 * Terminates the process and cleans up resources
 * @param status Exit status code
 */
void KernelExit(int status) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelExit: ERROR - no active process\n");
        return;
    }
    
    TracePrintf(2, "KernelExit called by PID %d with status %d\n", activeProcess->pid, status);
    
    // Save exit status
    activeProcess->status = TERMINATED;

    // Check if the terminating process is idle
    if (activeProcess->pid == 0) {
        TracePrintf(0, "KernelExit: Idle process called KernelExit(). Halting...\n");
        printf("Error: Idle process called KernelExit(). Halting...\n");
        Halt();
    }

    // Deal with this process' parent
    ProcessControlBlock *parent = activeProcess->parent;
    if (parent != NULL && parent->pid) {
        if (parent->status == BLOCKED) {
            TracePrintf(3, "KernelExit: Process %d exits and adds parent %d back to readyQueue\n", 
                        activeProcess->pid, parent->pid);
            RemoveSpecificPCB(blockedQueue, parent);
            AddToReadyQueue(parent);
        }

        // Add the child to the parent's exitQ (save status)
        struct exitNode *newExit = malloc(sizeof(struct exitNode));
        newExit->pid = activeProcess->pid;
        newExit->exit_status = status;
        newExit->next = NULL;
        TracePrintf(3, "KernelExit: Process %d exits and adds to parent %d's exitQ\n", activeProcess->pid, parent->pid);

        // Add new exit node to the PCB's exit queue
        struct exitNode *current_exit = parent->exitQ;
        if (current_exit) {
            while (current_exit->next) {    // Traverse to the end of the exit queue
                current_exit = current_exit->next;
            }
            current_exit->next = newExit;
        } 
        else {
            parent->exitQ = newExit;
        }
        TracePrintf(3, "KernelExit: Added to exitQ\n");
    }

    // Mark this process' children to be orphaned (parent = NULL)
    struct childNode *child = activeProcess->childQ;
    while (child) {
        ProcessControlBlock *child_process = child->pcb;
        if (child_process != NULL) {
            child_process->parent = NULL;  // Set parent to NULL
        }
        child = child->next;
    }
    TracePrintf(3, "KernelExit: Marked all child as orphan\n");

    // Get the next ready process
    ProcessControlBlock *next = GetNextReadyProcess();
    
    // Context switch to next process
    ContextSwitch(exitSwitch, &activeProcess->ctx, activeProcess, next);
}

/**
 * Waits for a child process to terminate
 * @param status_ptr Pointer to store child's exit status
 * @return PID of terminated child, ERROR if no children
 */
int KernelWait(int *status_ptr) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelWait: ERROR - no active process\n");
        return ERROR;
    }
    
    TracePrintf(2, "KernelWait called by PID %d\n", activeProcess->pid);

    // Check for terminated children
    // No running or exited child process, simply return ERROR
    if (activeProcess->children_num == 0 && activeProcess->exitQ == NULL) {
        TracePrintf(0, "KernelWait: ERROR - Parent process %d has no children\n", activeProcess->pid);
        return ERROR;
    }
    // Has children, but still running
    while (activeProcess->exitQ == NULL) {
        TracePrintf(3, "KernelWait: Parent process %d's children are all still running. Waiting...\n", activeProcess->pid);
        // Context switch to next process
        ProcessControlBlock *next = GetNextReadyProcess();
        ContextSwitch(waitSwitch, &activeProcess->ctx, activeProcess, next);
    }
    // Has exited children, or the parent switch back and finds its children finishes
    struct exitNode* exit = popExitQ(activeProcess);

    // Remove the exited process from parent's child count
    activeProcess->children_num--;

    TracePrintf(3, "KernelWait: Parent process %d has an exited child: pid %d with status %d\n", activeProcess->pid, exit->pid, exit->exit_status);
    *status_ptr = exit->exit_status;
    int exit_pid = exit->pid;

    // Free the exit node
    free(exit);

    return exit_pid;
}

/**
 * Returns the process ID of the current process
 * @return Current process ID
 */
int KernelGetPid(void) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelGetPid: WARNING - no active process, returning 0\n");
        return 0;
    }

    TracePrintf(2, "KernelGetPid called by PID %d\n", activeProcess->pid);
    return activeProcess->pid;
}

/**
 * Expands the heap memory of the process
 * @param addr New break address
 * @return 0 on success, ERROR on failure
 */
int KernelBrk(void *addr) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelBrk: ERROR - no active process\n");
        return ERROR;
    }
    
    TracePrintf(2, "KernelBrk called by PID %d with addr %p\n", activeProcess->pid, addr);
    
    // Use SetBrk function from memory.c
    int result = SetBrk(addr, activeProcess);
    return (result == 0) ? 0 : ERROR;
}

/**
 * Delays the process for a specified number of clock ticks
 * @param clock_ticks Number of clock ticks to delay
 * @return 0 on success, ERROR on failure
 */
int KernelDelay(int clock_ticks) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelDelay: ERROR - no active process\n");
        return ERROR;
    }
    
    if (clock_ticks < 0) {
        TracePrintf(0, "KernelDelay: ERROR - negative clock ticks\n");
        return ERROR;
    }
    
    if (clock_ticks == 0) {
        return 0;  // Return immediately
    }
    
    TracePrintf(2, "KernelDelay called by PID %d for %d ticks\n", activeProcess->pid, clock_ticks);
    
    // 1. Update process delay_ticks
    activeProcess->delay_ticks = clock_ticks;

    // 2. Context switch to another process
    ProcessControlBlock *next = GetNextReadyProcess();
    ContextSwitch(waitSwitch, &activeProcess->ctx, activeProcess, next);
    
    return 0;
}

/**
 * Reads data from a terminal
 * @param tty_id Terminal ID
 * @param buf Buffer to store data
 * @param len Maximum number of bytes to read
 * @return Number of bytes read on success, ERROR on failure
 */
int KernelTtyRead(int tty_id, void *buf, int len) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelTtyWrite: ERROR - no active process. Exiting...\n");
        printf("ERROR: No active process. Exiting...\n");
        return ERROR;
    }

    // Returns nothing from the terminal if len is 0 (p.17)
    if (len == 0) {
        TracePrintf(0, "KernelTtyRead: Process %d reads len 0\n", tty_id);
        return 0;
    }
    
    if (tty_id < 0 || tty_id >= NUM_TERMINALS) {
        TracePrintf(0, "KernelTtyRead: ERROR - Process %d calls TtyRead with invalid terminal ID %d. Exiting...\n", 
            activeProcess->pid, tty_id);
        printf("ERROR: Process %d calls TtyRead with invalid terminal ID %d. Exiting...\n", 
                activeProcess->pid, tty_id);
        return ERROR;
    }
    
    if (buf == NULL) {
        TracePrintf(0, "KernelTtyRead: ERROR - null buffer. Exiting...\n");
        printf("ERROR: Process %d calls TtyRead with null buffer. Exiting...\n", activeProcess->pid);
        return ERROR;
    }
    
    if (len < 0) {
        TracePrintf(0, "KernelTtyRead: ERROR - invalid length %d. Exiting...\n", len);
        printf("ERROR: Process %d calls TtyRead with invalid length %d. Exiting...\n", activeProcess->pid, len);
        return ERROR;
    }
    
    TracePrintf(0, "KernelTtyRead: TtyRead called by PID %d for terminal %d, len=%d\n", activeProcess->pid, tty_id, len);

    // Check if there is already ReceivedLine
    struct ReceivedLine *receivedLine = firstReceivedLine[tty_id];
    // There is a received line, copy it to the buffer
    if (receivedLine != NULL) {
        TracePrintf(0, "KernelTtyRead: firstReceivedLine's size is %d\n", receivedLine->size);

        // The first receivedLine is longer than this call is looking for
        // Then copy the first len bytes to the buffer, and update the receivedLine
        if (len < receivedLine->size) {
            memcpy(buf, (void*)receivedLine->data, len);
            receivedLine->size = receivedLine->size - len;
            memmove(receivedLine->data, receivedLine->data + len, receivedLine->size);           
            return len;
        }
        // The first receivedLine is shorter than this call is looking for
        // Then copy the whole receivedLine to the buffer, and free the receivedLine
        else {
            firstReceivedLine[tty_id] = receivedLine->next;
            memcpy(buf, receivedLine->data, receivedLine->size);
            free(receivedLine->original_data);
            len = receivedLine->size;
            free(receivedLine);
            return len;
        }
    }
    // No received line, block the process
    else {
        // Record the process's read request
        activeProcess->read_len = len;
        // Add the process into read_blocked queue
        TracePrintf(0, "KernelTtyRead: Terminal %d does not have available line, blocking process %d\n", tty_id, activeProcess->pid);
        AddToQueue(tty_read_blocked_queue[tty_id], activeProcess);
        
        // Context switch to another process
        ProcessControlBlock *next = GetNextReadyProcess();
        ContextSwitch(ttySwitch, &activeProcess->ctx, activeProcess, next);

        // After context switch, the process is unblocked and has a line
        // Copy the line to the buffer
        memcpy(buf, activeProcess->read_buf, activeProcess->read_result);
        return activeProcess->read_result;
    }
}

/**
 * Writes data to a terminal
 * @param tty_id Terminal ID
 * @param buf Buffer containing data to write
 * @param len Number of bytes to write
 * @return Number of bytes written on success, ERROR on failure
 */
int KernelTtyWrite(int tty_id, void *buf, int len) {
    if (activeProcess == NULL) {
        TracePrintf(0, "KernelTtyWrite: ERROR - no active process. Exiting...\n");
        printf("ERROR: No active process. Exiting...\n");
        return ERROR;
    }
    
    if (tty_id < 0 || tty_id >= NUM_TERMINALS) {
        TracePrintf(0, "KernelTtyWrite: ERROR - invalid terminal ID %d. Exiting...\n", tty_id);
        printf("ERROR: Process %d calls TtyWrite with invalid terminal ID %d. Exiting...\n", activeProcess->pid, tty_id);
        return ERROR;
    }
    
    if (buf == NULL) {
        TracePrintf(0, "KernelTtyWrite: ERROR - null buffer. Exiting...\n");
        printf("ERROR: Process %d calls TtyWrite with null buffer. Exiting...\n", activeProcess->pid);
        return ERROR;
    }
    
    if (len <= 0) {
        TracePrintf(0, "KernelTtyWrite: ERROR - invalid length %d. Exiting...\n", len);
        printf("ERROR: Process %d calls TtyWrite with invalid length %d. Exiting...\n", activeProcess->pid, len);
        return ERROR;
    }

    if (len > TERMINAL_MAX_LINE) {
        TracePrintf(0, "KernelTtyWrite: ERROR - length %d exceeds max line size. Exiting...\n", len);
        printf("ERROR: Process %d calls TtyWrite with length %d exceeds max line size. Exiting...\n", activeProcess->pid, len);
        return ERROR;
    }
    
    TracePrintf(0, "KernelTtyWrite: KernelTtyWrite called by PID %d for terminal %d, len=%d\n", activeProcess->pid, tty_id, len);
    
    // Check if the terminal is busy,
    // If it is, context switch to another process
    while (terminalTransmitBusy[tty_id] == 1) {
        TracePrintf(0, "KernelTtyWrite: Terminal %d is busy, blocking process %d\n", tty_id, activeProcess->pid);
        AddToQueue(tty_write_blocked_queue[tty_id], activeProcess);
        ProcessControlBlock *next = GetNextReadyProcess();
        ContextSwitch(ttySwitch, &activeProcess->ctx, activeProcess, next);
    }

    // Terminal is not busy, proceed with writing
    TracePrintf(0, "KernelTtyWrite: Terminal %d is not busy, proceeding process %d\n", tty_id, activeProcess->pid);
    TracePrintf(1, "KernelTtyWrite: Process %d writing %d bytes to terminal %d from virtual address %p\n", 
        activeProcess->pid, len, tty_id, (void*)buf);
    terminalTransmitBusy[tty_id] = 1; // Mark the terminal as busy
    TtyTransmit(tty_id, buf, len);
    return len;
}