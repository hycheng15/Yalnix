// >>>> THIS FILE IS ONLY A TEMPLATE FOR YOUR LoadProgram FUNCTION

// >>>> You MUST edit each place marked by ">>>>" below to replace
// >>>> the ">>>>" description with code for your kernel to implement the
// >>>> behavior described.  You might also want to save the original
// >>>> annotations as comments.

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <comp421/hardware.h>
#include <comp421/loadinfo.h>
#include <comp421/yalnix.h>
#include <stdint.h>

#include "pcb.h"

/*
 *  Load a program into the current process's address space.  The
 *  program comes from the Unix file identified by "name", and its
 *  arguments come from the array at "args", which is in standard
 *  argv format.
 *
 *  Returns:
 *      0 on success
 *     -1 on any error for which the current process is still runnable
 *     -2 on any error for which the current process is no longer runnable
 *
 *  This function, after a series of initial checks, deletes the
 *  contents of Region 0, thus making the current process no longer
 *  runnable.  Before this point, it is possible to return ERROR
 *  to an Exec() call that has called LoadProgram, and this function
 *  returns -1 for errors up to this point.  After this point, the
 *  contents of Region 0 no longer exist, so the calling user process
 *  is no longer runnable, and this function returns -2 for errors
 *  in this case.
 */
extern void FreeRegion0Pages(struct pte *userpt);

int LoadProgram(char *name, char **args, ExceptionInfo* programExceptionInfo, struct ProcessControlBlock* pcb) {
    int fd;
    int status;
    struct loadinfo li;
    char *cp;
    char *cp2;
    char **cpp;
    char *argbuf;
    int i;
    int argcount;
    int size;
    int text_npg;
    int data_bss_npg;
    int stack_npg;

    TracePrintf(0, "LoadProgram '%s', args %p\n", name, args);

    if (args == NULL) {
        TracePrintf(0, "LoadProgram: args in wrong format (cannot be NULL)\n");
        return (-1);
    }

    if ((fd = open(name, O_RDONLY)) < 0) {
        TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
        return (-1);
    }

    status = LoadInfo(fd, &li);
    TracePrintf(0, "LoadProgram: LoadInfo status %d\n", status);
    switch (status) {
        case LI_SUCCESS:
            break;
        case LI_FORMAT_ERROR:
            TracePrintf(0,
            "LoadProgram: '%s' not in Yalnix format\n", name);
            close(fd);
            return (-1);
        case LI_OTHER_ERROR:
            TracePrintf(0, "LoadProgram: '%s' other error\n", name);
            close(fd);
            return (-1);
        default:
            TracePrintf(0, "LoadProgram: '%s' unknown error\n", name);
            close(fd);
            return (-1);
    }
    TracePrintf(5, "text_size 0x%lx, data_size 0x%lx, bss_size 0x%lx\n",
	li.text_size, li.data_size, li.bss_size);
    TracePrintf(5, "entry 0x%lx\n", li.entry);

    /*
     *  Figure out how many bytes are needed to hold the arguments on
     *  the new stack that we are building.  Also count the number of
     *  arguments, to become the argc that the new "main" gets called with.
     */
    size = 0;
    for (i = 0; args[i] != NULL; i++) {
	    size += strlen(args[i]) + 1;
    }
    argcount = i;
    TracePrintf(5, "LoadProgram: size %d, argcount %d\n", size, argcount);

    /*
     *  Now save the arguments in a separate buffer in Region 1, since
     *  we are about to delete all of Region 0.
     */
    cp = argbuf = (char *)malloc(size);
    for (i = 0; args[i] != NULL; i++) {
        strcpy(cp, args[i]);
        cp += strlen(cp) + 1;
    }
  
    /*
     *  The arguments will get copied starting at "cp" as set below,
     *  and the argv pointers to the arguments (and the argc value)
     *  will get built starting at "cpp" as set below.  The value for
     *  "cpp" is computed by subtracting off space for the number of
     *  arguments plus 4 (for the argc value, a 0 (AT_NULL) to
     *  terminate the auxiliary vector, a NULL pointer terminating
     *  the argv pointers, and a NULL pointer terminating the envp
     *  pointers) times the size of each (sizeof(void *)).  The
     *  value must also be aligned down to a multiple of 8 boundary.
     */
    cp = ((char *)USER_STACK_LIMIT) - size;
    cpp = (char **)((unsigned long)cp & ((unsigned long)(-1) << 4));	/* align cpp */
    cpp = (char **)((unsigned long)cpp - ((argcount + 4) * sizeof(void *)));

    text_npg = li.text_size >> PAGESHIFT;
    data_bss_npg = UP_TO_PAGE(li.data_size + li.bss_size) >> PAGESHIFT;
    stack_npg = (USER_STACK_LIMIT - DOWN_TO_PAGE(cpp)) >> PAGESHIFT;

    TracePrintf(5, "LoadProgram: text_npg %d, data_bss_npg %d, stack_npg %d\n",
	    text_npg, data_bss_npg, stack_npg);

    /*
     *  Make sure we will leave at least one page between heap and stack
     */
    if (MEM_INVALID_PAGES + text_npg + data_bss_npg + stack_npg + 1 + KERNEL_STACK_PAGES >= PAGE_TABLE_LEN) {
        TracePrintf(0, "LoadProgram: program '%s' size too large for VM\n", name);
        free(argbuf);
        close(fd);
        return (-1);
    }

    /*
     *  And make sure there will be enough physical memory to
     *  load the new program.
     */
    // >>>> The new program will require text_npg pages of text,
    // >>>> data_bss_npg pages of data/bss, and stack_npg pages of
    // >>>> stack.  In checking that there is enough free physical
    // >>>> memory for this, be sure to allow for the physical memory
    // >>>> pages already allocated to this process that will be
    // >>>> freed below before we allocate the needed pages for
    // >>>> the new program being loaded.
    // if (>>>> not enough free physical memory) {
	// TracePrintf(0,
	//     "LoadProgram: program '%s' size too large for physical memory\n",
	//     name);
	// free(argbuf);
	// close(fd);
	// return (-1);
    // }

    // >>>> Initialize sp for the current process to (void *)cpp.
    // >>>> The value of cpp was initialized above.
    programExceptionInfo->sp = (void *)cpp;

    /*
     *  Free all the old physical memory belonging to this process,
     *  but be sure to leave the kernel stack for this process (which
     *  is also in Region 0) alone.
     */
    // >>>> Loop over all PTEs for the current process's Region 0,
    // >>>> except for those corresponding to the kernel stack (between
    // >>>> address KERNEL_STACK_BASE and KERNEL_STACK_LIMIT).  For
    // >>>> any of these PTEs that are valid, free the physical memory
    // >>>> memory page indicated by that PTE's pfn field.  Set all
    // >>>> of these PTEs to be no longer valid.

    int free_pages = CountFreePhysicalPages();
    int current_allocated_pages = CountAllocatedUserPages();

    if (text_npg + data_bss_npg + stack_npg - current_allocated_pages > free_pages)
    {
        TracePrintf(0, "LoadProgram: program '%s' size too large for physical memory\n", name);
        free(argbuf);
        close(fd);
        return (-1);
    }
    TracePrintf(5, "LoadProgram: enough free physical memory\n");
    FreeRegion0Pages(pcb->page_table);

    /*
     *  Fill in the page table with the right number of text,
     *  data+bss, and stack pages.  We set all the text pages
     *  here to be read/write, just like the data+bss and
     *  stack pages, so that we can read the text into them
     *  from the file.  We then change them read/execute.
     */

    // >>>> Leave the first MEM_INVALID_PAGES number of PTEs in the
    // >>>> Region 0 page table unused (and thus invalid)

    TracePrintf(5, "LoadProgram: initialize Region 0 page table\n");
    for (int i = 0; i < MEM_INVALID_PAGES; i++) {
        pcb->page_table[i].valid = 0;
    }

    /* First, the text pages */
    // >>>> For the next text_npg number of PTEs in the Region 0
    // >>>> page table, initialize each PTE:
    // >>>>     valid = 1
    // >>>>     kprot = PROT_READ | PROT_WRITE
    // >>>>     uprot = PROT_READ | PROT_EXEC
    // >>>>     pfn   = a new page of physical memory
    for (int i = MEM_INVALID_PAGES; i < MEM_INVALID_PAGES + text_npg; i++) {
        int pfn = AllocatePage();
        if(pfn == -1) {
            TracePrintf(0, "LoadProgram: not enough free physical memory\n");
            for (int j = MEM_INVALID_PAGES; j < i; j++) {
                FreePage(pcb->page_table[j].pfn);
                pcb->page_table[j].valid = 0;
            }
            // Returns -2 because original region 0 is freed and cannot resume original process
            return -2;  
        }
        pcb->page_table[i].valid = 1;
        pcb->page_table[i].kprot = PROT_READ | PROT_WRITE;
        pcb->page_table[i].uprot = PROT_READ | PROT_EXEC;
        pcb->page_table[i].pfn = pfn;
    }
    TracePrintf(5, "LoadProgram: Initialized pages for text\n");

    /* Then the data and bss pages */
    // >>>> For the next data_bss_npg number of PTEs in the Region 0
    // >>>> page table, initialize each PTE:
    // >>>>     valid = 1
    // >>>>     kprot = PROT_READ | PROT_WRITE
    // >>>>     uprot = PROT_READ | PROT_WRITE
    // >>>>     pfn   = a new page of physical memory
    for (int i = MEM_INVALID_PAGES + text_npg; i < MEM_INVALID_PAGES + text_npg + data_bss_npg; i++) {
        int pfn = AllocatePage();
        if (pfn == -1) {
            TracePrintf(0, "LoadProgram: not enough free physical memory for data/bss pages\n");

            // Rollback: Only free pages allocated in this section (data + BSS pages)
            for (int j = MEM_INVALID_PAGES + text_npg; j < i; j++) {
                FreePage(pcb->page_table[j].pfn);
                pcb->page_table[j].valid = 0;
            }
            
            // Also free previously allocated text pages (rollback to full safety)
            for (int j = MEM_INVALID_PAGES; j < MEM_INVALID_PAGES + text_npg; j++) {
                FreePage(pcb->page_table[j].pfn);
                pcb->page_table[j].valid = 0;
            }

            // Returns -2 because original region 0 is freed and cannot resume original process
            return -2;
        }
        pcb->page_table[i].valid = 1;
        pcb->page_table[i].kprot = PROT_READ | PROT_WRITE;
        pcb->page_table[i].uprot = PROT_READ | PROT_WRITE;
        pcb->page_table[i].pfn = pfn;
    }
    TracePrintf(5, "LoadProgram: Initialized pages for data and bss\n");

    /* And finally the user stack pages */
    // >>>> For stack_npg number of PTEs in the Region 0 page table
    // >>>> corresponding to the user stack (the last page of the
    // >>>> user stack *ends* at virtual address USER_STACK_LIMIT),
    // >>>> initialize each PTE:
    // >>>>     valid = 1
    // >>>>     kprot = PROT_READ | PROT_WRITE
    // >>>>     uprot = PROT_READ | PROT_WRITE
    // >>>>     pfn   = a new page of physical memory
    int stack_base = (USER_STACK_LIMIT >> PAGESHIFT) - stack_npg;
    for(int i = stack_base; i < (USER_STACK_LIMIT >> PAGESHIFT); i++) {
        int pfn = AllocatePage();
        if(pfn == -1) {
            TracePrintf(0, "LoadProgram: not enough free physical memory for stack pages\n");

            // Rollback: Free only stack pages that were allocated
            for (int j = stack_base; j < i; j++) {
                FreePage(pcb->page_table[j].pfn);
                pcb->page_table[j].valid = 0;
            }
    
            // Also free previously allocated text and data/bss pages (rollback everything)
            for (int j = MEM_INVALID_PAGES; j < MEM_INVALID_PAGES + text_npg; j++) {
                FreePage(pcb->page_table[j].pfn);
                pcb->page_table[j].valid = 0;
            }
    
            for (int j = MEM_INVALID_PAGES + text_npg; j < MEM_INVALID_PAGES + text_npg + data_bss_npg; j++) {
                FreePage(pcb->page_table[j].pfn);
                pcb->page_table[j].valid = 0;
            }
            
            // Returns -2 because original region 0 is freed and cannot resume original process
            return -2;
        }
        pcb->page_table[i].valid = 1;
        pcb->page_table[i].kprot = PROT_READ | PROT_WRITE;
        pcb->page_table[i].uprot = PROT_READ | PROT_WRITE;
        pcb->page_table[i].pfn = pfn;
    }

    TracePrintf(5, "LoadProgram: Initialized pages for stack\n");

    /*
     * Update the user heap break and stack break
     */
    pcb->heap_brk = (void *)(uintptr_t)((MEM_INVALID_PAGES + data_bss_npg + text_npg) << PAGESHIFT);
    pcb->stack_brk = (void *)(uintptr_t)(stack_base << PAGESHIFT);
    
    TracePrintf(5, "LoadProgram: Updated heap_brk and stack_brk\n");

    /*
     *  All pages for the new address space are now in place.  Flush
     *  the TLB to get rid of all the old PTEs from this process, so
     *  we'll be able to do the read() into the new pages below.
     */
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    /*
     *  Read the text and data from the file into memory.
     */
    if (read(fd, (void *)MEM_INVALID_SIZE, li.text_size+li.data_size) != (ssize_t)(li.text_size+li.data_size)) {
        TracePrintf(0, "LoadProgram: couldn't read for '%s'\n", name);
        free(argbuf);
        close(fd);
        // Free all allocated pages before exiting
        for (int i = MEM_INVALID_PAGES; i < MEM_INVALID_PAGES + text_npg; i++) {
            FreePage(pcb->page_table[i].pfn);
            pcb->page_table[i].valid = 0;
        }

        for (int i = MEM_INVALID_PAGES + text_npg; i < MEM_INVALID_PAGES + text_npg + data_bss_npg; i++) {
            FreePage(pcb->page_table[i].pfn);
            pcb->page_table[i].valid = 0;
        }

        int stack_base = (USER_STACK_LIMIT >> PAGESHIFT) - stack_npg;
        for (int i = stack_base; i < (USER_STACK_LIMIT >> PAGESHIFT); i++) {
            FreePage(pcb->page_table[i].pfn);
            pcb->page_table[i].valid = 0;
        }
        // >>>> Since we are returning -2 here, this should mean to
        // >>>> the rest of the kernel that the current process should
        // >>>> be terminated with an exit status of ERROR reported
        // >>>> to its parent process.
        return (-2);
    }
    TracePrintf(5, "LoadProgram: Read the text and data from the file into memory\n");
    close(fd);			/* we've read it all now */

    /*
     *  Now set the page table entries for the program text to be readable
     *  and executable, but not writable.
     */
    // >>>> For text_npg number of PTEs corresponding to the user text
    // >>>> pages, set each PTE's kprot to PROT_READ | PROT_EXEC.
    for (int i = MEM_INVALID_PAGES; i < MEM_INVALID_PAGES + text_npg; i++) {
        pcb->page_table[i].kprot = PROT_READ | PROT_EXEC;
    }
    TracePrintf(5, "LoadProgram: Set text' kprot to PROT_READ | PROT_EXEC\n");
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    /*
     *  Zero out the bss
     */
    memset((void *)(MEM_INVALID_SIZE + li.text_size + li.data_size), '\0', li.bss_size);

    /*
     *  Set the entry point in the exception frame.
     */
    //  >>>> Initialize pc for the current process to (void *)li.entry
    programExceptionInfo->pc = (void *)li.entry;
    TracePrintf(5, "LoadProgram: Initialize pc for the current process to (void *)li.entry %p\n", programExceptionInfo->pc);

    /*
     *  Now, finally, build the argument list on the new stack.
     */
    *cpp++ = (char *)(intptr_t)argcount;		/* the first value at cpp is argc */
    cp2 = argbuf;
    for (i = 0; i < argcount; i++) {      /* copy each argument and set argv */
        *cpp++ = cp;
        strcpy(cp, cp2);
        cp += strlen(cp) + 1;
        cp2 += strlen(cp2) + 1;
    }
    free(argbuf);
    *cpp++ = NULL;	/* the last argv is a NULL pointer */
    *cpp++ = NULL;	/* a NULL pointer for an empty envp */
    *cpp++ = 0;		/* and terminate the auxiliary vector */
    
    TracePrintf(5, "LoadProgram: cpp %p\n", cpp);
    TracePrintf(5, "LoadProgram: sp %p\n", programExceptionInfo->sp);
    
    /*
     *  Initialize all regs[] registers for the current process to 0,
     *  initialize the PSR for the current process also to 0.  This
     *  value for the PSR will make the process run in user mode,
     *  since this PSR value of 0 does not have the PSR_MODE bit set.
     */
    // >>>> Initialize regs[0] through regs[NUM_REGS-1] for the
    // >>>> current process to 0.
    // >>>> Initialize psr for the current process to 0.
    for (int i = 0; i < NUM_REGS; i++) {
        programExceptionInfo->regs[i] = 0;
    }
    programExceptionInfo->psr = 0;
    TracePrintf(5, "LoadProgram: Initialize psr for the current process to 0\n");
    
    TracePrintf(1, "LoadProgram: Load complete\n");

    return (0);
}
