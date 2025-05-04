/*
* Purpose: Handles all traps, including system calls, memory exceptions, and hardware interrupts. 
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

#define min(a,b) \
({ __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
  _a < _b ? _a : _b; })

// The return value from the kernel call should be return to the user process in regs[0] field
void TrapKernel(ExceptionInfo *info){
    switch(info->code) {
        case YALNIX_FORK:
            TracePrintf(2, "Process %d trapped in YALNIX_FORK - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelFork();
            break;
        case YALNIX_EXEC:
            TracePrintf(2, "Process %d trapped in YALNIX_EXEC - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelExec(info);
            break;
        case YALNIX_EXIT:
            TracePrintf(2, "Process %d trapped in YALNIX_EXIT - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            KernelExit(info->regs[1]);
            break;
        case YALNIX_WAIT:
            TracePrintf(2, "Process %d trapped in YALNIX_WAIT - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelWait((int *)info->regs[1]);
            break;
        case YALNIX_GETPID:
            TracePrintf(2, "Process %d trapped in YALNIX_GETPID - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelGetPid();
            break;
        case YALNIX_BRK:
            TracePrintf(2, "Process %d trapped in YALNIX_BRK - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelBrk((void *)info->regs[1]);
            break;
        case YALNIX_DELAY:
            TracePrintf(2, "Process %d trapped in YALNIX_DELAY - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelDelay(info->regs[1]);
            break;
        case YALNIX_TTY_READ:
            TracePrintf(2, "Process %d trapped in YALNIX_TTY_READ - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelTtyRead(info->regs[1], (void *)info->regs[2], info->regs[3]);
            break;
        case YALNIX_TTY_WRITE:
            TracePrintf(2, "Process %d trapped in YALNIX_TTY_WRITE - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = KernelTtyWrite(info->regs[1], (void *)info->regs[2], info->regs[3]);
            break;
        default:
            TracePrintf(0, "Unknown system call  - with vector(%d), code(%d), addr(%p), psr(%d), pc(%p), sp(%p)\n", 
                activeProcess->pid, info->vector, info->code, info->addr, info->psr, info->pc, info->sp);
            info->regs[0] = ERROR;
    }
}

/**
 * Handles clock interrupts for process scheduling
 * Implementss round-robin scheduling by switching between processes
 * @param info Exception information structure
 */
void TrapClock(ExceptionInfo *info) {
    TracePrintf(2, "TrapClock: Process %d: vector(%d), code(%d), addr(%p), pc(%p), sp(%p)\n", 
        activeProcess->pid, info->vector, info->code, info->addr, info->pc, info->sp);
    
    // Increment clock ticks
    clock_ticks++;

    // Decrement the delay_ticks for the blocked process
    // Move the process to the ready queue if delay_ticks is 0
    ProcessControlBlock *curr = blockedQueue->head;
    while (curr != NULL) {
        ProcessControlBlock *next = curr->next; // Save next before possible removal

        if (curr->delay_ticks >= 0) {
            TracePrintf(5, "TrapClock: decrement delay_ticks for process %d from %d to %d\n", curr->pid, curr->delay_ticks, curr->delay_ticks - 1);
        }
        curr->delay_ticks--;
        if (curr->delay_ticks == 0) {
            TracePrintf(3, "TrapClock: PID %d delay done, moving to ready queue\n", curr->pid);
            RemoveSpecificPCB(blockedQueue, curr);
            AddToReadyQueue(curr);
        }
        curr = next; // Move to next process
    }

    if (activeProcess == NULL) {
        TracePrintf(0, "TrapClock: ERROR - no active process\n");
        return;
    }

    // Schedule the process in a round-robin fashion
    // Each process runs 2 clock ticks before switching
    if (activeProcess == idle) {
        // If the active process is idle, switch to the next ready process directly
        ProcessControlBlock *next = GetNextReadyProcess();
        if (next->pid == 0) {
            TracePrintf(3, "TrapClock: No other process ready, idle process continues\n");
            return; // No need to switch
        }
        TracePrintf(3, "TrapClock: Current running process is idle, switch to next PID %d\n", next->pid);
        ContextSwitch(generalSwitch, &activeProcess->ctx, activeProcess, next);
        return; 
    }
    else {
        activeProcess->time_slice--;
        // This process has run for 2 clock ticks, needs to switch
        if (activeProcess->time_slice == 0) {
            TracePrintf(3, "TrapClock: Time slice expired for process %d\n", activeProcess->pid);
            ProcessControlBlock *next = GetNextReadyProcess();
            // If the only process ready is idle, then we don't need to switch
            if (next->pid == 0) {
                TracePrintf(3, "TrapClock: Next process is idle, reset process %d's time slice and don't switch\n", 
                    activeProcess->pid);
                activeProcess->time_slice = TIME_SLICE; // Reset time slice
                return;
            }
            // Context switch to the next process
            // This will also adjust readyQueue and blockedQueue
            TracePrintf(3, "TrapClock: Switching from PID %d to PID %d\n", activeProcess->pid, next->pid);
            ContextSwitch(generalSwitch, &activeProcess->ctx, activeProcess, next);
        }
    }
}

/**
 * Handles illegal instruction traps
 * Terminates the process if it executes an illegal instruction
 * @param info Exception information structure
 */
void TrapIllegal(ExceptionInfo *info) {
    TracePrintf(2, "TrapIllegal: Process %d: vector(%d), code(%d), addr(%p), pc(%p), sp(%p)\n", 
        activeProcess->pid, info->vector, info->code, info->addr, info->pc, info->sp);
    
    if (activeProcess == NULL) {
        TracePrintf(0, "TrapIllegal: Error - No active process\n");
        return;
    }

    // Print error message and terminate the process
    char *error_msg;
    if (info->code == TRAP_ILLEGAL_ILLOPC) error_msg = "Illegal opcode";
	else if (info->code == TRAP_ILLEGAL_ILLOPN) error_msg = "Illegal operand";
	else if (info->code == TRAP_ILLEGAL_ILLADR) error_msg = "Illegal address mode";
	else if (info->code == TRAP_ILLEGAL_ILLTRP) error_msg = "Illegal software trap";
	else if (info->code == TRAP_ILLEGAL_PRVOPC) error_msg = "Privileged opcode";
    else if (info->code == TRAP_ILLEGAL_PRVREG) error_msg = "Privileged register";
    else if (info->code == TRAP_ILLEGAL_COPROC) error_msg = "Coprocessor error";
    else if (info->code == TRAP_ILLEGAL_BADSTK) error_msg = "Bad stack";
    else if (info->code == TRAP_ILLEGAL_KERNELI) error_msg = "Linux kernel sent SIGILL";
    else if (info->code == TRAP_ILLEGAL_USERIB) error_msg = "Received SIGILL or SIGBUS from user";
    else if (info->code == TRAP_ILLEGAL_ADRALN) error_msg = "Invalid address alignment";
    else if (info->code == TRAP_ILLEGAL_ADRERR) error_msg = "Non-existent physical address";
    else if (info->code == TRAP_ILLEGAL_OBJERR) error_msg = "Object-specific HW error";
    else if (info->code == TRAP_ILLEGAL_KERNELB) error_msg = "Linux kernel sent SIGBUS";
    
    printf("ERROR: Process %d terminating due to %s\n", activeProcess->pid, error_msg);
    TracePrintf(0, "TrapIllegal: Illegal instruction in PID %d. Terminating process with error message %s.\n", activeProcess->pid, error_msg);
    
    KernelExit(ERROR);  // Force exit with ERROR status as specified in p.20
}

/**
 * Handles memory access traps
 * Expands stack if needed, otherwise terminates the process
 * @param info Exception information structure
 */
void TrapMemory(ExceptionInfo *info) {
    TracePrintf(2, "TrapMemory: Process %d: vector(%d), code(%d), addr(%p), pc(%p), sp(%p)\n", 
            activeProcess->pid, info->vector, info->code, info->addr, info->pc, info->sp);
    
    if (activeProcess == NULL) {
        TracePrintf(0, "TrapMemory: No active process\n");
        return;
    }

    char *error_msg;

    if (info->code != TRAP_MEMORY_MAPERR) {
        if (info->code == TRAP_MEMORY_ACCERR) error_msg = "Protection violation at addr";
        else if (info->code == TRAP_MEMORY_KERNEL) error_msg = "Linux kernel sent SIGSEGV";
        else if (info->code == TRAP_MEMORY_USER) error_msg = "Received SIGSEGV from user";
        TracePrintf(0, "TrapMemory: TrapMemory in PID %d. Terminating process with error message %s.\n", activeProcess->pid, error_msg);
        printf("ERROR: Process %d terminating due to %s\n", activeProcess->pid, error_msg);
        KernelExit(ERROR);  // Force exit with ERROR status as specified in p.20
    }

    unsigned long addr = (unsigned long)info->addr;
    error_msg = "No mapping at addr";

    // Check if the address is within the stack or heap boundaries
    // addr should satisfy the following 3 conditions, or the process is terminated
    // 1. Be in Region 0 and below the user stack limit
    if (addr > USER_STACK_LIMIT) {
        TracePrintf(0, "TrapMemory: Error - Invalid memory access in PID %d at address %p, above USER_STACK_LIMIT. Terminating process.\n", 
            activeProcess->pid, info->addr);
        printf("ERROR: Invalid memory access in PID %d at address %p (above USER_STACK_LIMIT). Terminating process.\n",
            activeProcess->pid, info->addr);
        KernelExit(ERROR);  // Force exit with ERROR status as specified in p.20
        return;
    }
    // 2. Below the current allocated memory for the stack
    if (addr > (unsigned long)activeProcess->stack_brk) {
        TracePrintf(0, "TrapMemory: Error - Current stack_brk of PID %d is %p, cannot expand to higher address %p. Terminating process.\n", 
            activeProcess->pid, activeProcess->stack_brk, info->addr);
        printf("ERROR: Current stack_brk of PID %d is %p, cannot expand to higher address %p. Terminating process.\n", 
            activeProcess->pid, activeProcess->stack_brk, info->addr);
        KernelExit(ERROR);
        return;
    }
    // 3. Above the current heap break (and should leave one page of red zone)
    if ((unsigned long)(DOWN_TO_PAGE(addr) - PAGESIZE) < (unsigned long)activeProcess->heap_brk) {
        TracePrintf(0, "TrapMemory: Stackoverflow - current heap_brk of PID %d is %p, cannot expand stack to %p. Terminating process.\n", 
            activeProcess->pid, activeProcess->heap_brk, info->addr);
        fprintf(stderr, "ERROR: Stackoverflow - current heap_brk of PID %d is %p, cannot expand stack to %p. Terminating process.\n", 
            activeProcess->pid, activeProcess->heap_brk, info->addr);
        KernelExit(ERROR);
        return;
    }

    // The address is within the safe zone, expand the stack
    // Call the ExpandStack function to try expanding the stack
    // If it fails (returns -1), terminate the process
    if (ExpandStack(info) == -1) {
        TracePrintf(0, "TrapMemory: Failed to expand stack for PID %d. Terminating process.\n", activeProcess->pid);
        printf("ERROR: Failed to expand stack for PID %d. Terminating process.\n", activeProcess->pid);
        activeProcess->status = TERMINATED;
        KernelExit(ERROR);
        return;
    }
}

/**
 * Handles math error traps
 * Terminates the process on math errors (divide by zero, overflow, etc.)
 * @param info Exception information structure
 */
void TrapMath(ExceptionInfo *info) {
    TracePrintf(2, "TrapMath: Process %d: vector(%d), code(%d), addr(%p), pc(%p), sp(%p)\n", 
        activeProcess->pid, info->vector, info->code, info->addr, info->pc, info->sp);
    
    if (activeProcess == NULL) {
        TracePrintf(0, "TrapMath: No active process\n");
        return;
    }
    
    // Print error message and terminate the process
    char *error_msg;
    if (info->code == TRAP_MATH_INTDIV) error_msg = "Integer divide by zero";
    else if (info->code == TRAP_MATH_INTOVF) error_msg = "Integer overflow";
    else if (info->code == TRAP_MATH_FLTDIV) error_msg = "Floating point divide by zero";
    else if (info->code == TRAP_MATH_FLTOVF) error_msg = "Floating point overflow";
    else if (info->code == TRAP_MATH_FLTUND) error_msg = "Floating point underflow";
    else if (info->code == TRAP_MATH_FLTRES) error_msg = "Floating point inexact result";
    else if (info->code == TRAP_MATH_FLTINV) error_msg = "Invalid floating point operation";
    else if (info->code == TRAP_MATH_FLTSUB) error_msg = "Floating point subscript out of range";
    else if (info->code == TRAP_MATH_KERNEL) error_msg = "Linux kernel sent SIGFPE";
    else if (info->code == TRAP_MATH_USER) error_msg = "Received SIGFPE from user";
    
    printf("ERROR: Process %d terminating due to %s\n", activeProcess->pid, error_msg);
    TracePrintf(0, "TrapMath: Math operation error in PID %d. Terminating process with error message %s.\n", activeProcess->pid, error_msg);
    KernelExit(ERROR);  // Force exit with ERROR status as specified in p.20
}

/**
 * Handles terminal input interrupts
 * Processes data received from terminals
 * @param info Exception information structure
 */
void TrapTtyReceive(ExceptionInfo *info) {
    int terminal_id = info->code;

    TracePrintf(2, "TrapTtyReceive: vector(%d), terminal_id(%d)\n", info->vector, terminal_id);
    
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) {
        TracePrintf(0, "TrapTtyReceive: Invalid terminal ID %d\n", terminal_id);
        return;
    }
    
    TracePrintf(0, "TrapTtyReceive: Received data on terminal %d\n", terminal_id);
    
    // Get the received data into a temporary buffer
    char temp_buf[TERMINAL_MAX_LINE];
    int bytes_read = TtyReceive(terminal_id, temp_buf, TERMINAL_MAX_LINE);
    
    TracePrintf(0, "TrapTtyReceive: Read %d bytes from terminal %d\n", bytes_read, terminal_id);
    
    // Create a new ReceivedLine structure to store the received data
    struct ReceivedLine *newLine = malloc(sizeof(struct ReceivedLine));
    newLine->size = bytes_read;
    newLine->next = NULL;
    newLine->data = malloc(bytes_read);
    newLine->original_data = newLine->data;
    memcpy(newLine->data, temp_buf, bytes_read);

    // Add the new line to the firstReceivedLine linked list
    if (firstReceivedLine[terminal_id] == NULL) {
        firstReceivedLine[terminal_id] = newLine;
    }
    else {
        // Find the end of the linked list and add the new line
        struct ReceivedLine *curr = firstReceivedLine[terminal_id];
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = newLine;
    }

    // Check if any process is waiting to read from this terminal
    ProcessControlBlock *waiting_process = RemoveFromQueue(tty_read_blocked_queue[terminal_id]);

    if (waiting_process != NULL) {
        // If a process is waiting, unblock it and let it continue reading
        TracePrintf(0, "TrapTtyTransmit: Unblocking process %d waiting to read from terminal %d\n", waiting_process->pid, terminal_id);
        struct ReceivedLine *curr = firstReceivedLine[terminal_id];
        // Copy the data from the first received line to the process's buffer
        waiting_process->read_result = 0; // Reset read result
        for (int i = 0; i < min(waiting_process->read_len, curr->size); i++) {
            waiting_process->read_buf[i] = curr->data[i];
            waiting_process->read_result++;
        }

        curr->size -= waiting_process->read_result;
        if (curr->size > 0) {
            // Shift the remaining data in the buffer
            memmove(curr->data, curr->data + waiting_process->read_result, curr->size);
        }
        else {
            // If the line is empty, free it
            firstReceivedLine[terminal_id] = curr->next;
            free(curr->original_data);
            free(curr);
        }
    
        // Context switch to the waiting process and make it write to its buffer
        ContextSwitch(ttySwitch, &activeProcess->ctx, activeProcess, waiting_process);
            
    } 
    else {
        TracePrintf(0, "TrapTtyTransmit: No process waiting to read from terminal %d\n", terminal_id);
    }
}

/**
 * Handles terminal output interrupts
 * Processes completion of terminal write operations
 * @param info Exception information structure
 */
void TrapTtyTransmit(ExceptionInfo *info) {
    int terminal_id = info->code;

    TracePrintf(2, "TrapTtyTransmit: Process %d: terminal_id(%d), pc(%p), sp(%p)\n", 
        activeProcess->pid, terminal_id, info->pc, info->sp);
    
    if (terminal_id < 0 || terminal_id >= NUM_TERMINALS) {
        TracePrintf(0, "TrapTtyTransmit: Invalid terminal ID %d\n", terminal_id);
        return;
    }

    // Mark the terminal as not busy
    terminalTransmitBusy[terminal_id] = 0;
    TracePrintf(0, "TrapTtyTransmit: Transmission complete on terminal %d\n", terminal_id);
    
    // Check if any process is waiting to write to this terminal
    ProcessControlBlock *waiting_process = RemoveFromQueue(tty_write_blocked_queue[terminal_id]);
    
    if (waiting_process != NULL) {
        // If a process is waiting, unblock it and let it continue writing
        TracePrintf(0, "TrapTtyTransmit: Unblocking process %d waiting to write to terminal %d\n", waiting_process->pid, terminal_id);
        // Update process state and add to ready queue
        AddToReadyQueue(waiting_process);    
    } 
    else {
        TracePrintf(0, "TrapTtyTransmit: No process waiting to write to terminal %d\n", terminal_id);
    }

    ProcessControlBlock *next = GetNextReadyProcess();
    ContextSwitch(generalSwitch, &activeProcess->ctx, activeProcess, next);
}