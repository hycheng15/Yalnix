#include "pcb.h"

/**
 * Adds a PCB to the end of the readyQueue
 * Change the status of the process to be READY
 * Reset the process's time slice to TIME_SLICE
 * @param pcb Process control block to add
 */
void AddToReadyQueue(ProcessControlBlock *pcb) {
    TracePrintf(3, "AddToReadyQueue: Adding process %d into ready queue\n", pcb->pid);
    if (readyQueue == NULL || pcb == NULL || pcb->pid == 0) {
        TracePrintf(0, "AddToReadyQueue: cannot add process to readyQueue\n");
        return;
    }

    // Set the status of the process to be READY
    pcb->status = READY;

    // Reset the process's time slice
    pcb->time_slice = TIME_SLICE;

    // Reset PCB's delay_ticks
    pcb->delay_ticks = 0;

    ProcessControlBlock *debug = readyQueue->head;
    TracePrintf(10, "--- Before adding ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");
    
    // Check if the PCB is already in the queue
    ProcessControlBlock *cur = readyQueue->head;
    while (cur != NULL) {
        if (cur == pcb) {
            TracePrintf(0, "AddToReadyQueue: Warning - PCB %d is already in the readyQueue\n", pcb->pid);
            return;
        }
        cur = cur->next;
    }

    // Reset PCB's next and prev pointer
    pcb->prev = NULL;
    pcb->next = NULL;

    // If queue is empty, set head and tail to this PCB
    if (readyQueue->head == NULL) {
        pcb->prev = NULL;
        readyQueue->head = pcb;
        readyQueue->tail = pcb;
        TracePrintf(5, "AddToReadyQueue: ReadyQueue is empty, process %d added to the head\n", pcb->pid);
    } else {
        // Otherwise, add to the end of the queue
        TracePrintf(5, "AddToReadyQueue: readyQueue->tail PID is %d\n", readyQueue->tail->pid);
        pcb->prev = readyQueue->tail;
        readyQueue->tail->next = pcb;
        readyQueue->tail = pcb;
        TracePrintf(5, "AddToReadyQueue: Process %d added to the end of readyQueue\n", pcb->pid);
    }

    debug = readyQueue->head;
    TracePrintf(10, "--- After adding ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");
}

/**
 * Adds a PCB to the end of the blockedQueue
 * Change the process's status to be BLOCKED
 * @param pcb Process control block to add
 */
void AddToBlockedQueue(ProcessControlBlock *pcb) {
    TracePrintf(3, "AddToBlockedQueue: Adding process %d into blocked queue\n", pcb->pid);
    if (blockedQueue == NULL || pcb == NULL) {
        return;
    }

    // Set the status of the process to be BLOCKED
    pcb->status = BLOCKED;

    ProcessControlBlock *debug = blockedQueue->head;
    TracePrintf(10, "--- Before adding ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");

    // Check if the PCB is already in the queue
    ProcessControlBlock *cur = blockedQueue->head;
    while (cur != NULL) {
        if (cur == pcb) {
            TracePrintf(0, "AddToBlockedQueue: Warning - PCB %d is already in the blockedQueue\n", pcb->pid);
            return;
        }
        cur = cur->next;
    }

    // Reset PCB's next and prev pointer
    pcb->prev = NULL;
    pcb->next = NULL;
    
    // If queue is empty, set head and tail to this PCB
    if (blockedQueue->head == NULL) {
        pcb->prev = NULL;
        blockedQueue->head = pcb;
        blockedQueue->tail = pcb;
    } else {
        // Otherwise, add to the end of the queue
        pcb->prev = blockedQueue->tail;
        blockedQueue->tail->next = pcb;
        blockedQueue->tail = pcb;
    }

    debug = blockedQueue->head;
    TracePrintf(10, "--- After adding ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");
}

/**
 * Removes and returns a PCB from the head of the queue
 * @param q Target queue
 * @return Removed PCB, or NULL if queue is empty
 */
ProcessControlBlock *RemoveFromQueue(struct PCBqueue *q) {
    if (q == readyQueue) {
        TracePrintf(3, "RemoveFromQueue: Removing process from ready queue\n");
    } 
    else if (q == blockedQueue) {
        TracePrintf(3, "RemoveFromQueue: Removing process from blocked queue\n");
    } 

    ProcessControlBlock *debug = q->head;
    TracePrintf(10, "--- Before removing ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");

    if (q == NULL) {
        TracePrintf(0, "RemoveFromQueue: Error - Cannot remove process from NULL queue\n");
        return NULL;
    }

    if (q->head == NULL) {
        TracePrintf(0, "RemoveFromQueue: Error - Cannot remove process from empty queue\n");
        return NULL;
    }

    // Get PCB from the head of the queue
    ProcessControlBlock *pcb = q->head;
    
    // Update queue head
    q->head = pcb->next;
    
    // If queue is now empty, update tail to NULL as well
    if (q->head == NULL) {
        q->tail = NULL;
    } else {
        // Otherwise, update new head's prev pointer
        q->head->prev = NULL;
    }
    
    // Reset connection pointers of the removed PCB
    pcb->next = NULL;
    pcb->prev = NULL;
    
    debug = q->head;
    TracePrintf(10, "--- After removing ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");

    return pcb;
}

/**
 * Removes a specific PCB from the queue
 * @param q Target queue
 * @param pcb PCB to remove
 * @return 1 if successfully removed, 0 otherwise
 */
int RemoveSpecificPCB(struct PCBqueue *q, ProcessControlBlock *pcb) {
    if (q == readyQueue) {
        TracePrintf(3, "RemoveSpecificPCB: Removing process %d from ready queue\n", pcb->pid);
    } 
    else if (q == blockedQueue) {
        TracePrintf(3, "RemoveSpecificPCB: Removing process %d from blocked queue\n", pcb->pid);
    } 

    if (q == NULL || pcb == NULL || q->head == NULL) {
        TracePrintf(0, "RemoveSpecificPCB: Error - Cannot remove process from certain queue\n");
        return 0;
    }

    ProcessControlBlock *debug = q->head;
    TracePrintf(10, "--- Before removing ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");

    ProcessControlBlock *current = q->head;
    while (current != NULL && current != pcb) {
        current = current->next;
    }

    if (current == NULL) {
        TracePrintf(1, "RemoveSpecificPCB: PCB not found in queue\n");
        return 0;  // PCB not in queue
    }

    if (current == q->head) {
        q->head = current->next;
        if (q->head == NULL) {
            q->tail = NULL;
        } else {
            q->head->prev = NULL;
        }
    } else if (current == q->tail) {
        q->tail = current->prev;
        q->tail->next = NULL;
    } else {
        current->prev->next = current->next;
        current->next->prev = current->prev;
    }
    current->next = NULL;
    current->prev = NULL;

    debug = q->head;
    TracePrintf(10, "--- After removing ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");

    return 1;
}

/**
 * Checks if a queue is empty
 * @param q Queue to check
 * @return 1 if queue is empty, 0 otherwise
 */
int IsQueueEmpty(struct PCBqueue *q) {
    return (q->head == NULL);
}

/**
 * Return the next ready process to run from readyQueue
 * If there is not next ready process, returns the idle process
 * @return the next ready process' pcb
 */
ProcessControlBlock *GetNextReadyProcess() {
    TracePrintf(3, "GetNextReadyProcess: Get next ready process from readyQueue\n");

    // Get the next process from ReadyQueue, or set the next to be idle
    if (IsQueueEmpty(readyQueue)) {
        TracePrintf(3, "GetNextReadyProcess: No process in readyQueue, return idle process\n");
        return idle;
    }
    else {
        return readyQueue->head;
    }
}

/**
 * Return the process' first exit child 
 * @param pcb target process
 * @return The first exit child in the FIFO exitQ of the process pcb
 */
exitNode *popExitQ(ProcessControlBlock *pcb) {
    TracePrintf(3, "popExitQ: Pop next exit child process from process %d\n", pcb->pid);
    if (pcb->exitQ == NULL) {
        TracePrintf(3, "popExitQ: Error - Process %d does not have exited child\n", pcb->pid);
        return NULL;
    }
    struct exitNode *head = pcb->exitQ;
	pcb->exitQ = head->next;
	return head;
}

/**
 * Add a PCB to the end of a specified queue
 * @param queue target queue
 * @param pcb PCB to add
 */
void AddToQueue(PCBqueue *queue, ProcessControlBlock *pcb) {
    if (queue == NULL || pcb == NULL) {
        TracePrintf(0, "AddToQueue: ERROR - NULL queue or PCB\n");
        return;
    }
    
    TracePrintf(0, "AddToQueue: Adding process %d to queue at %p\n", pcb->pid, queue);

    ProcessControlBlock *debug = queue->head;
    TracePrintf(10, "--- Before adding ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");

    // Check if the PCB is already in the queue
    ProcessControlBlock *cur = queue->head;
    while (cur != NULL) {
        if (cur == pcb) {
            TracePrintf(0, "AddToQueue: Warning - PCB %d is already in the queue\n", pcb->pid);
            return;
        }
        cur = cur->next;
    }

    // Resest PCB's next and prev pointer
    pcb->prev = NULL;
    pcb->next = NULL;
    
    if (queue->head == NULL) {
        // Emty queue, set head and tail to this PCB
        queue->head = pcb;
        queue->tail = pcb;
        pcb->prev = NULL;
        TracePrintf(0, "AddToQueue: Queue was empty, process %d is now head and tail\n", pcb->pid);
    } else {
        // Add the PCB to the end of the queue
        pcb->prev = queue->tail;
        queue->tail->next = pcb;
        queue->tail = pcb;
        TracePrintf(0, "AddToQueue: Added process %d to end of queue\n", pcb->pid);
    }

    debug = queue->head;
    TracePrintf(10, "--- After adding ---\n");
    while (debug != NULL) {
        TracePrintf(10, "Process %d\n", debug->pid);
        debug = debug->next;
    }
    TracePrintf(10, "------\n");
}

/**
 * Free the specified process' page table and pcb
 * @param pcb pcb of the process to be destroyed
 */
void DestroyProcess(ProcessControlBlock *pcb) {
    TracePrintf(3, "DestroyProcess: Destroying process %d, free page table and pcb\n", pcb->pid);
    if (pcb == NULL) return;
    
    // Release the page table
    if (pcb->page_table) {
        DeletePageTable(pcb->page_table);
        pcb->page_table = NULL;
    }
    
    // Remove the process from queue
    if (pcb->prev) pcb->prev->next = pcb->next;
    if (pcb->next) pcb->next->prev = pcb->prev;

    // Free the childNodes in the childQ
    childNode *child = pcb->childQ;
    while (child) {
        childNode *temp = child;
        child = child->next;
        free(temp);
    }
    pcb->childQ = NULL;
    
    // We don't need to free the exitQ
    // Since when the parent process calls Wait()
    // It will pop (remove) the first exitNode from the exitQ and we will also free it

    // Release the pcb structure
    free(pcb);
}