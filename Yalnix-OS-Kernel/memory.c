/*
* Purpose: Handles page allocation, freeing, and virtual memory setup.
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
#include <stdint.h>

#include "pcb.h"


struct pageTableRecord *firstPageTableRecord;       // Keep track of all the current page tables
void *void_page;                                    // A void page for page copy

/**
 * Initializes the physical memory manager
 * @param pmem_size Total physical memory size (in bytes)
 */
void InitializeMemory(unsigned int pmem_size) {
    TracePrintf(1, "InitializeMemory: pmem_size = %d\n", pmem_size);

    num_physical_pages = pmem_size / PAGESIZE;
    if(num_physical_pages > MAX_PHYS_PAGES) {
        TracePrintf(0, "Warning: pmem_size exceeds MAX_PHYS_PAGES\n");
        num_physical_pages = MAX_PHYS_PAGES;
    }

    // initialize all pages as free
    free_frame_map = malloc(num_physical_pages * sizeof(int));
    memset(free_frame_map, 0, num_physical_pages);

    for (int i = 0; i < MEM_INVALID_PAGES; i++) {
        free_frame_map[i] = 1;
    }

    // Mark the memory already in use by the kernel
    // Spec p.28: be careful not to include any memory that is already in use by your kernel
    // Kernel heap
    for (int i = VMEM_1_BASE >> PAGESHIFT; i < UP_TO_PAGE(kernel_brk) >> PAGESHIFT; i++) {
        free_frame_map[i] = 1;
    }
    // Kernel stack
    for (int i = KERNEL_STACK_BASE >> PAGESHIFT; i < KERNEL_STACK_LIMIT >> PAGESHIFT; i++) {
        free_frame_map[i] = 1;
    }

    TracePrintf(5, "Physical memory initialized: %d pages (%d bytes)\n", num_physical_pages, pmem_size);
}

/**
 * Allocates a physical page and returns its frame number
 * @return Frame number on success, -1 on failure
 */
int AllocatePage() {
    for (int i = 0; i < num_physical_pages; i++) {
        if (!free_frame_map[i]) {  // Find a free frame
            free_frame_map[i] = 1; // Mark as allocated
            TracePrintf(6, "AllocatePage: Allocated physical page pfn %d\n", i);
            return i;
        }
    }

    TracePrintf(0, "AllocatePage: No free pages available\n");
    return -1;  // No free pages available
}

/**
 * Marks a page as free
 * @param pfn Page frame number to free
 */
void FreePage(int pfn) {
    if (pfn < 0 || pfn >= (int)num_physical_pages) {
        TracePrintf(6, "FreePage: Invalid page frame number %d\n", pfn);
        return;
    }

    if (free_frame_map[pfn] == 0) {
        TracePrintf(6, "FreePage: Page %d is already free\n", pfn);
        return;
    }
    free_frame_map[pfn] = 0;  // Mark as free
    TracePrintf(6, "FreePage: Freed page %d\n", pfn);
}

/**
 * Counts the number of free physical pages in the system
 * @return Number of free physical pages
 */
int CountFreePhysicalPages() {
    int free_pages = 0;
    for (int i = 0; i < (int)num_physical_pages; i++) {
        if (free_frame_map[i] == 0) {
            free_pages++;
        }
    }
    return free_pages;
}

/**
 * Counts the current allocated pages in region 0 (excluding kernel stack)
 * @return Number of allocated pages in region 0 (excluding kernel stack)
 */
int CountAllocatedUserPages() {
    int page_count = 0;
    for (int i = MEM_INVALID_PAGES; i < KERNEL_STACK_BASE >> PAGESHIFT; i++) {
        if(activeProcess->page_table[i].valid) {
            page_count++;
        }
    }
    return page_count;
}

/*
 * Create the first pageTableRecord for user processes' page table
 */
void initFirstPTRecord() {
    TracePrintf(1, "initFirstPTRecord: Start initializing first pageTableRecord\n");
    struct pageTableRecord *newPTRecord = malloc(sizeof(struct pageTableRecord));
    
    // Get the TOP first address of the page in region 1
    void *region1_base = (void *)(VMEM_LIMIT - PAGESIZE);
    unsigned int pfn = (VMEM_LIMIT - PAGESIZE) / PAGESIZE;
    // Mark the physical frame as used
    free_frame_map[pfn] = 1;

    newPTRecord->region1_base = region1_base;
    newPTRecord->pfn = pfn;
    newPTRecord->isTopFull = 0;
    newPTRecord->isBottomFull = 0;
    newPTRecord->next = NULL;
    
    // Set up the vpn to pfn mapping in the kernel page table
    int vpn = (long)(region1_base - VMEM_1_BASE) / PAGESIZE;
    
    kernelpt[vpn].valid = 1;
    kernelpt[vpn].pfn = pfn;
    TracePrintf(6, "initFirstPTRecord: PFN %d's free_frame_map is %d\n", pfn, free_frame_map[pfn]);
    kernelpt[vpn].kprot = PROT_READ | PROT_WRITE;
    kernelpt[vpn].uprot = PROT_NONE;
  
    firstPageTableRecord = newPTRecord;
    TracePrintf(5, "initFirstPTRecord: First pageTableRecord lies in region one with vpn %d, region1_base %p, and uses pfn %d\n", vpn, region1_base, pfn);
  }

/**
 * Create and allocate a new page table for a user process
 * This will check the current PageTableRecord to see if there is space
 * If not, it will create a new PageTableRecord and allocate a new page in region 1
 * @return Pointer to the new page table
 */
struct pte* createPageTable() {
    TracePrintf(1, "createPageTable: Try allocating new page table in current PageTableRecord\n");
    struct pageTableRecord *current = firstPageTableRecord;

    while (1) {
        TracePrintf(5, "CreatePageTable: current region1_base %p, pfn %d, next %p, Top full? %s, Bottom full? %s\n",
                    current->region1_base, current->pfn, current->next,
                    current->isTopFull ? "Yes" : "No",
                    current->isBottomFull ? "Yes" : "No");
    
        // Check if the top slot is available
        if (!current->isTopFull) {
            struct pte *newPT = (struct pte *)((long)current->region1_base + PAGE_TABLE_SIZE);
            current->isTopFull = 1;
            TracePrintf(4, "CreatePageTable: Used top of page to create page table at %p with physicall address %p\n", 
                newPT, current->pfn * PAGESIZE + PAGE_TABLE_SIZE);
            return newPT;
        }
    
        // Otherwise, check if the bottom slot is available
        if (!current->isBottomFull) {
            struct pte *newPT = (struct pte *)current->region1_base;
            current->isBottomFull = 1;
            TracePrintf(4, "CreatePageTable: Used bottom of page to create page table at %p with physicall address %p\n", 
                newPT, current->pfn * PAGESIZE);
            return newPT;
        }
    
        // Neither slot is available; move to the next pageTableRecord if it exists
        if (current->next) {
            current = current->next;
        }
        else {
            // No available pageTableRecord, create a new one after the while
            break;
        }
    }

    TracePrintf(5, "createPageTable: No available pageTableRecord in the linked list, create new available pageTableRecord\n");

    struct pageTableRecord *newPTRecord = malloc(sizeof(struct pageTableRecord));
    if (newPTRecord == NULL){
        TracePrintf(0, "createPageTable: Kernel failed malloc for new pageTableRecord. Halting...\n");
        printf("ERROR: Kernel failed malloc for new pageTableRecord. Halting...\n");
        Halt();
    }

    long pfn = AllocatePage();
    if (pfn < 0){
        TracePrintf(0, "createPageTable: Kernel failed allocating new physical page for new pageTableRecord. Halting...\n");
        printf("ERROR: Kernel failed allocating new physical page for new pageTableRecord. Halting...\n");
        Halt();
    }
    void *region1_base = (void *)((long)current->region1_base - PAGESIZE);
    
    newPTRecord->region1_base = region1_base;
    newPTRecord->pfn = pfn;
    TracePrintf(6, "createPageTable: PFN %d's free_frame_map is %d\n", pfn, free_frame_map[pfn]);
    newPTRecord->isTopFull = 1;
    newPTRecord->isBottomFull = 0;
    newPTRecord->next = NULL;

    int vpn = (long)(region1_base - VMEM_1_BASE) / PAGESIZE;
    kernelpt[vpn].valid = 1;
    kernelpt[vpn].pfn = pfn;
    kernelpt[vpn].kprot = PROT_READ | PROT_WRITE;
    kernelpt[vpn].uprot = PROT_NONE;
    current->next = newPTRecord;    // Add the new record to the linked list

    // Assign the top PTRecord to the new page table
    struct pte *newPageTable = (struct pte *)((long)region1_base + PAGE_TABLE_SIZE);
    TracePrintf(4, "createPageTable: Created new pageTableRecord and used top of page to create page table at %p with physicall addr %p\n", 
        newPageTable, (long)pfn * PAGESIZE + PAGE_TABLE_SIZE);
    return newPageTable;
}

/*
 * Initializes the page table structure, used in KernelStart
 * This function allocates page tables for Region 1 (kernel),
 * and sets up the basic mapping relationships
 */
void InitializeKernelPageTable() {
    TracePrintf(1, "InitializeKernelPageTable: Initializing kernel page table\n");

    kernelpt = (struct pte *)malloc(PAGE_TABLE_SIZE);
    TracePrintf(4, "InitializeKernelPageTable: kernelpt is at %p\n", (void*) kernelpt);

    if (kernelpt == NULL) {
        TracePrintf(0, "InitializeKernelPageTable: ERROR - failed to allocate memory for kernelpt. Halting...\n");
        printf("ERROR: Kernel failed allocating new physical page for kernelpt. Halting...\n");
        Halt();
    }

    // Set up the reserved area in the kernel page table (MEM_INVALID_PAGES)
    for (int i = 0; i < MEM_INVALID_PAGES; i++) {
        kernelpt[i].valid = 0;
    }

    // Initialize all page table entries as invalid
    memset(kernelpt, 0, PAGE_TABLE_SIZE);

    // Kernel code and data regions (inside region 1)
    long kernel_text_end = (long)&_etext;
    int kernel_text_npages = (kernel_text_end - (long)VMEM_1_BASE) / PAGESIZE;
    
    TracePrintf(5, "InitializeKernelPageTable: kernel text ends at %p, covers %d pages\n", 
                (void*)kernel_text_end, kernel_text_npages);

    long kernel_heap_npages = ((long)kernel_brk - kernel_text_end) / PAGESIZE;
    TracePrintf(5, "InitializeKernelPageTable: kernel heap ends at %p, covers %d pages\n", 
        kernel_brk, kernel_heap_npages);

    for (int i = 0; i < PAGE_TABLE_LEN; i++) {
        // Map kernel text region (read+execute)
        if (i < kernel_text_npages) {
            kernelpt[i].valid = 1;
            kernelpt[i].pfn = ((long)VMEM_1_BASE / PAGESIZE) + i; // Direct mapping
            free_frame_map[kernelpt[i].pfn] = 1; // Mark the physical frame as used
            kernelpt[i].kprot = PROT_READ | PROT_EXEC;
            kernelpt[i].uprot = PROT_NONE;
            TracePrintf(6, "InitializeKernelPageTable: vpn is %d (addr is %p), pfn is %d (addr is %p)\n", 
                i, (i * PAGESIZE + VMEM_1_BASE), kernelpt[i].pfn, (kernelpt[i].pfn * PAGESIZE));
            TracePrintf(6, "InitializeKernelPageTable: pfn %d's free_frame_map is %d\n", 
                kernelpt[i].pfn, free_frame_map[kernelpt[i].pfn]);
        }
        // Map kernel data region and heap (read+write)
        else if (i < kernel_text_npages + kernel_heap_npages) {
            kernelpt[i].valid = 1;
            kernelpt[i].pfn = ((long)VMEM_1_BASE / PAGESIZE) + i;  // Direct mapping
            free_frame_map[kernelpt[i].pfn] = 1; // Mark the physical frame as used
            kernelpt[i].kprot = PROT_READ | PROT_WRITE;
            kernelpt[i].uprot = PROT_NONE;
            TracePrintf(6, "InitializeKernelPageTable: vpn is %d (addr is %p), pfn is %d (addr is %p)\n", 
                i, (i * PAGESIZE + VMEM_1_BASE), kernelpt[i].pfn, (kernelpt[i].pfn * PAGESIZE));
            TracePrintf(6, "InitializeKernelPageTable: pfn %d's free_frame_map is %d\n", 
                kernelpt[i].pfn, free_frame_map[kernelpt[i].pfn]);
        }
        // Set up unused regions
        else {
            kernelpt[i].valid = 0;
            kernelpt[i].kprot = PROT_NONE;
            kernelpt[i].uprot = PROT_NONE;
        }
    }

    TracePrintf(5, "InitializeKernelPageTable: kernelpt initialized\n");
}

/**
 * Enables virtual memory
 * This function sets up the page table base registers and enables virtual memory
 */
void EnableVirtualMemory() {    
    // Enable virtual memory
    WriteRegister(REG_VM_ENABLE, 1);
    isVMEnabled = 1;
    
    TracePrintf(1, "EnableVirtualMemory: virtual memory enabled\n");
}

/**
 * Expands the user stack
 * Handles memory exceptions when a process accesses an unallocated stack region
 * by automatically expanding the stack
 * @param info Exception information structure
 * @return 0 on success, -1 on failure
 */
int ExpandStack(ExceptionInfo *info) {
    TracePrintf(4, "ExpandStack: Expanding user stack\n");
    if (activeProcess == NULL) {
        TracePrintf(0, "ExpandStack: ERROR - no active process\n");
        return -1;
    }
    
    unsigned long addr = (unsigned long)info->addr;
    unsigned long page_addr = DOWN_TO_PAGE(addr);
    
    // Check if there is enough physical pages
    int num_needed_pages = ((unsigned long)activeProcess->stack_brk - page_addr) / PAGESIZE;
    if (num_needed_pages > CountFreePhysicalPages()) {
        TracePrintf(0, "ExpandStack: ERROR - not enough physical pages\n");
        return -1;
    }
    
    // Allocate physical memory for the new stack pages
    for (unsigned long p = page_addr; p < (unsigned long)activeProcess->stack_brk; p += PAGESIZE) {
        int pfn = AllocatePage();
        
        // Calculate virtual page number
        unsigned int vpn = (p - VMEM_0_BASE) >> PAGESHIFT;
        
        // Set up page table entry
        activeProcess->page_table[vpn].valid = 1;
        activeProcess->page_table[vpn].pfn = pfn;
        activeProcess->page_table[vpn].kprot = PROT_READ | PROT_WRITE;
        activeProcess->page_table[vpn].uprot = PROT_READ | PROT_WRITE;
        
        // Flush the TLB for this page
        WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)p);
    }
        
    // Update the stack boundary
    activeProcess->stack_brk = (void *)page_addr;
    TracePrintf(4, "ExpandStack: stack expanded to %p\n", (void *)page_addr);
    return 0;
}

/**
 * Expands or contracts the user heap
 * Implements the core functionality of the KernelBrk() system call
 * expand the heap or shrink the user heap
 * prevent the user heap from overlapping with the user stack
 * maintain the guard page between the user heap and the user stack
 * @param addr New brk address
 * @param pcb Process control block
 * @return 0 on success, -1 on failure
 */
int SetBrk(void *addr, ProcessControlBlock *pcb) {
    TracePrintf(4, "SetBrk: Expanding user heap\n");
    if (pcb == NULL) {
        TracePrintf(0, "SetBrk: ERROR - null PCB\n");
        return -1;
    }
    
    unsigned long new_brk = (unsigned long)UP_TO_PAGE(addr);
    unsigned long old_brk = (unsigned long)pcb->heap_brk;
    
    // Check validity
    if (new_brk < MEM_INVALID_SIZE) {
        TracePrintf(0, "SetBrk: ERROR - address below MEM_INVALID_SIZE\n");
        return -1;
    }
    
    // Define a guard page (one page gap) between heap and stack
    if (new_brk >= (unsigned long)(pcb->stack_brk - PAGESIZE)) {
        TracePrintf(0, "SetBrk: ERROR - would overlap with guard page or stack\n");
        return -1;
    }
    
    if (new_brk > old_brk) {
        // Expand heap
        
        // Check if there is enough physical pages
        int num_needed_pages = (new_brk - old_brk) / PAGESIZE;
        if (num_needed_pages > CountFreePhysicalPages()) {
            TracePrintf(0, "SetBrk: ERROR - not enough physical pages\n");
            return -1;
        }

        for (unsigned long p = old_brk; p < new_brk; p += PAGESIZE) {
            int pfn = AllocatePage();
            
            // Calculate virtual page number
            unsigned int vpn = (p - VMEM_0_BASE) >> PAGESHIFT;
            
            // Set up page table entry
            activeProcess->page_table[vpn].valid = 1;
            activeProcess->page_table[vpn].pfn = pfn;
            activeProcess->page_table[vpn].kprot = PROT_READ | PROT_WRITE;
            activeProcess->page_table[vpn].uprot = PROT_READ | PROT_WRITE;
            
            // Flush the TLB for this page
            WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)p);
        }
    } 
    else if (new_brk < old_brk) {
        // Contract heap

        for (unsigned long p = new_brk; p < old_brk; p += PAGESIZE) {
            unsigned int vpn = (p - VMEM_1_BASE) >> PAGESHIFT;
            if (activeProcess->page_table[vpn].valid) {
                FreePage(activeProcess->page_table[vpn].pfn);
                activeProcess->page_table[vpn].valid = 0;
                
                // Flush the TLB for this page
                WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)p);
            }
        }
    }
    
    // Update heap boundary
    pcb->heap_brk = (void *)new_brk;
    
    TracePrintf(4, "SetBrk: heap boundary set to %p\n", addr);
    return 0;
}

/**
 * Copy the content of every valid page 
 * from one process’s Region 0 address space (src_pt) to another (dest_pt)
 * Allocating new physical pages in the destination
 * @param src_pt Source page table
 * @param dest_pt Destination page table
 */
void CopyPage(struct pte *src_pt, struct pte *dest_pt) {
    TracePrintf(4, "CopyPage: copy valid pages in page table %p to page table %p\n", 
        (void*)src_pt, (void*)dest_pt);
    TracePrintf(4, "CopyPage: Kernel stack pages are in range %p (vpn %d) to %p (vpn %d)\n", 
            (void*)KERNEL_STACK_BASE, KERNEL_STACK_BASE >> PAGESHIFT, (void*)KERNEL_STACK_LIMIT, KERNEL_STACK_LIMIT >> PAGESHIFT);
    TracePrintf(4, "CopyPage: void_page pfn is at %d\n", (long)((void*)void_page - VMEM_1_BASE) >> PAGESHIFT); 
    
    int uprot;
    int kprot;
    // Loop through each entry in the source page table
    for (long vpn = 0; vpn < VMEM_0_LIMIT >> PAGESHIFT; vpn++) {
        if (src_pt[vpn].valid) {
            // Record the uprot and kprot for this entry
            uprot = src_pt[vpn].uprot;
            kprot = src_pt[vpn].kprot;
            // Copy the page content to the void page
            TracePrintf(6, "CopyPage: Copying page from address %p (pfn %d, vpn %d) to void_page %p\n", 
                (void*)(vpn * PAGESIZE), vpn, src_pt[vpn].pfn , void_page);
            memcpy(void_page, (void*)(vpn * PAGESIZE), PAGESIZE);
            
            // Change to the destination page table (change REG0)
            changeReg0(dest_pt);

            // Set up new page table entry
            dest_pt[vpn].valid = 1;
            dest_pt[vpn].pfn = AllocatePage();
            // Copy the page content from the void page to the new page
            // The MMU will copy the content in virtual memory to the physical memory
            memcpy((void*)(vpn * PAGESIZE), void_page, PAGESIZE);
            TracePrintf(6, "CopyPage: Copying page from void_page %p to address %p (pfn %d, vpn %d)\n", 
                void_page, (void*)(vpn * PAGESIZE), dest_pt[vpn].pfn, vpn);
            dest_pt[vpn].uprot = uprot;
            dest_pt[vpn].kprot = kprot;
   
            // Change back to the source page table
            changeReg0(src_pt);
			TracePrintf(6, "CopyPage: Finished copy one page - VPN %d into pfn %d\n", 
                vpn, dest_pt[vpn].pfn);
        }
    }
    TracePrintf(4, "CopyPage: Finished copying all pages\n");
}

/**
 * Deletes a process's page table and related physical memory
 * Used to clean up resources when a process terminates
 * @param pt Page table to delete
 */
void DeletePageTable(struct pte *pt) {
    if (pt == NULL) return;
    TracePrintf(4, "DeletePageTable: Deleting page table at %p\n", (void*)pt);

    // Free all physical pages associated with valid page table entries
    for (int vpn = 0; vpn < PAGE_TABLE_LEN; vpn++) {
        if (pt[vpn].valid) {
            // Be careful not to free the kernel stack pages of idle process
            if (pt[vpn].pfn >= KERNEL_STACK_BASE >> PAGESHIFT) {
                TracePrintf(5, "DeletePageTable: Skipping kernel stack page of idle process at pfn %d\n", pt[vpn].pfn);
                continue;
            }
            FreePage(pt[vpn].pfn);
            pt[vpn].valid = 0;
        }
    }

    // Deal with the pageTableRecord
    // Find which half of the page in region 1 the page table is in
    void* region1_base = (void*)DOWN_TO_PAGE(pt);
    int isBottom = (pt == region1_base);
    TracePrintf(5, "DeletePageTable: Page table %p, isBottom %d\n", pt, isBottom);

    // Loop through the linked list of pageTableRecords

    struct pageTableRecord* current = firstPageTableRecord;
    struct pageTableRecord* previous = NULL;
    while (current != NULL) {
        // Found the record this page table used
        if (current->region1_base == region1_base) {
            if (isBottom) {
                current->isBottomFull = 0;
                TracePrintf(5, "DeletePageTable: Mark bottom of %p empty\n", region1_base);
            }
            else {
                current->isTopFull = 0;
                TracePrintf(5, "DeletePageTable: Mark top of %p empty\n", region1_base);
            }
            // If the page record is completely free and is the last record, free it
            if (!current->isBottomFull && !current->isTopFull && current->next == NULL) {
                TracePrintf(5, "DeletePageTable: Page with base %p is completely free\n", region1_base);
                FreePage(current->pfn);     // Free the physical page
                free(current);              // Free the pageTableRecord
                previous->next = NULL;      // Mark the previous record's next as the last
                TracePrintf(5, "DeletePageTable: Clear kernel pte at vpn %d\n", ((long)region1_base - VMEM_1_BASE) >> PAGESHIFT);
                kernelpt[(long)region1_base >> PAGESHIFT].valid = 0;
                TracePrintf(5, "DeletePageTable: Flush TLB for cleared kernel pte at vpn %d\n", ((long)region1_base - VMEM_1_BASE) >> PAGESHIFT);
                WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)region1_base);
            }
            return;
        }
        previous = current;
        current = current->next;
    }
    
    TracePrintf(0, "DeletePageTable: Error - cannot find pt %p in PageRecords. Halting...\n", (void*)pt);
    printf("ERROR: Kernel cannot find pt %p in PageRecords thus cannot delete pt. Halting...\n", (void*)pt);
	Halt();  
}

// Free all pages in Region 0, except for the kernel stack
void FreeRegion0Pages(struct pte *userpt) {
    TracePrintf(4, "FreeRegion0Pages: free pages in Region 0\n");
    for (int i = MEM_INVALID_PAGES; i < KERNEL_STACK_BASE >> PAGESHIFT; i++) {
        if (userpt[i].valid) {
            FreePage(userpt[i].pfn);
            userpt[i].valid = 0;
        }
    }
    TracePrintf(5, "FreeRegion0Pages: Region 0 page cleanup complete\n");
}