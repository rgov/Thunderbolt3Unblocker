//
//  xnu_override.c
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 2/2/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

#include <kern/task.h>
#include <libkern/OSMalloc.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <machine/machine_routines.h>
#include <os/log.h>
#include <string.h>

#include <Zydis/Zydis.h>

#include "xnu_override.h"


#define LOG_PREFIX "xnu_override: "


// Private function we need to be able to get the kernel_map
vm_map_t get_task_map(task_t);


// Basic idea copied from mach_override/mach_override.c
#define kOriginalInstructionsSize 32
#define kIslandJumpOffset (kOriginalInstructionsSize + 2)
#define kPatchJumpOffset 2

char kIslandTemplate[] = {
    // kOriginalInstructionsSize nop instructions so that we
    // should have enough space to host original instructions
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    
    // Now our code to jump to the desired place
    // XXX This clobbers %rax, which is a caller-saved register
    0x48, 0xB8, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, // mov %rax, ...
    0xFF, 0xE0,                                                 // jmp %rax
};

char kPatchTemplate[] = {
    // Our code to jump to the desired place
    0x48, 0xB8, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, // mov %rax, ...
    0xFF, 0xE0,                                                 // jmp %rax
};

typedef struct BranchIsland {
    void *target;
    unsigned int insn_bytes;
    char instructions[sizeof(kIslandTemplate)];
    struct BranchIsland *nextIsland; // linked list for efficient unloading
} BranchIsland;

BranchIsland *firstIsland = NULL;

static OSMallocTag tag = NULL;


// From xnu/osfmk/proc_reg.h
#define CR0_WP 0x00010000  /* i486: Write-protect kernel access */

static inline uintptr_t get_cr0(void)
{
    uintptr_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r" (cr0));
    return(cr0);
}

static inline void set_cr0(uintptr_t value)
{
    __asm__ volatile("mov %0, %%cr0" : : "r" (value));
}

static inline void flush_tlb(void) {
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r" (cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r" (cr3));
}


static void dump(void *addr, int count) {
    for (int i = 0; i < count; i ++) {
        os_log(OS_LOG_DEFAULT, "%02x", ((uint8_t *)addr)[i]);
    }
    os_log(OS_LOG_DEFAULT, "\n");
}


static void disable_write_protection(void) {
    // Disable write protection
    /*
     TODO: Can we get some sort of lock so that we don't risk someone else
     changing cr0 at the same time?
     */
    os_log_debug(OS_LOG_DEFAULT, LOG_PREFIX "Disabling kernel write protection\n");
    set_cr0(get_cr0() & ~CR0_WP);
}

static void enable_write_protection(void) {
    // Re-enable write protection
    os_log_debug(OS_LOG_DEFAULT, LOG_PREFIX "Re-enabling kernel write protection\n");
    set_cr0(get_cr0() | CR0_WP);
}


kern_return_t xnu_override(void *target, const void *replacement, void **original) {
    if (!tag)
        tag = OSMalloc_Tagalloc("branch_island", OSMT_DEFAULT);
    
    // Allocate a branch island
    os_log_debug(OS_LOG_DEFAULT, LOG_PREFIX "Creating branch island\n");
    
    BranchIsland *island = OSMalloc(PAGE_SIZE, tag);
    island->target = target;
    island->nextIsland = NULL;
    bcopy(kIslandTemplate, &island->instructions, sizeof(island->instructions));
    
    // Now we will use the disassembler to copy instructions over one by one
    // until we have enough room for our jump
    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
  
    island->insn_bytes = 0;
    void *insn_ptr = &island->instructions;
    while (island->insn_bytes < sizeof(kPatchTemplate)) {
        ZydisStatus zerr;
        ZydisDecodedInstruction instruction;
        zerr = ZydisDecoderDecodeBuffer(
            &decoder,
            target + island->insn_bytes,
            64, /* always assume there are 64 bytes left */
            (uint64_t)target + island->insn_bytes,
            &instruction
        );
        if (!ZYDIS_SUCCESS(zerr)) {
            os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Cannot disassemble instruction, aborting: %d\n", zerr);
            goto fail;
        }
        
        island->insn_bytes += instruction.length;
        if (island->insn_bytes > kOriginalInstructionsSize) {
            os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Out of space in branch island, aborting\n");
            goto fail;
        }
        
        // TODO: Detect
        if (instruction.meta.category == ZYDIS_CATEGORY_RET) {
            os_log(OS_LOG_DEFAULT, LOG_PREFIX "Patch target may be too short, proceed cautiously\n");
        }
        
        bcopy(instruction.data, insn_ptr, instruction.length);
        insn_ptr += instruction.length;
    }
    
    // Ok, the branch island is almost ready, we just need to insert the address
    // we want it to jump to, which will be the first instruction after the ones
    // we copied into the island.
    uint64_t jumpTo = (uint64_t)target + island->insn_bytes;
    bcopy(&jumpTo, &island->instructions[kIslandJumpOffset], sizeof(uint64_t));

    // The branch island is ready. Switch from writable to executable.
    kern_return_t err = vm_protect(get_task_map(kernel_task), (vm_address_t)island, PAGE_SIZE, false, VM_PROT_EXECUTE | VM_PROT_READ);
    if (err != KERN_SUCCESS) {
        os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Failed to mark island executable, aborting\n");
        goto fail;
    }
    
    // Now we need to patch the target to insert a jump to the replacement code
    boolean_t ints = ml_set_interrupts_enabled(false);
    disable_write_protection();
    jumpTo = (uint64_t)replacement;
    bcopy(kPatchTemplate, target, sizeof(kPatchTemplate));
    bcopy(&jumpTo, target + kPatchJumpOffset, sizeof(uint64_t));
    enable_write_protection();
    ml_set_interrupts_enabled(ints);
    
    // Flush TLB to evict caches
    flush_tlb();
    
    // Return a pointer to the branch island, which is how to call the original
    // function.
    *original = &island->instructions;
    
    // Add this patch to the linked list
    if (firstIsland == NULL) {
        firstIsland = island;
    } else {
        BranchIsland *islandPtr = firstIsland;
        while (islandPtr->nextIsland != NULL) {
            islandPtr = islandPtr->nextIsland;
        }
        islandPtr->nextIsland = island;
    }
    
    os_log(OS_LOG_DEFAULT, LOG_PREFIX "Patch applied\n");
    return KERN_SUCCESS;
    
fail:
    OSFree(island, PAGE_SIZE, tag);
    return KERN_ABORTED;
}


kern_return_t xnu_unpatch(const void *target) {
    os_log(OS_LOG_DEFAULT, LOG_PREFIX "Reverting patch\n");
    
    // Scan through the list of branch islands for this target
    BranchIsland *island = firstIsland;
    BranchIsland *prevIsland = NULL;
    while (island != NULL && island->target != target) {
        prevIsland = island;
        island = island->nextIsland;
    }
    
    if (!island) {
        os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Unpatch target not found\n");
        return KERN_ABORTED;
    }
    
    // Update the linked list to skip over this island
    if (prevIsland == NULL) {
        firstIsland = island->nextIsland;
    } else {
        prevIsland->nextIsland = island->nextIsland;
    }
    
    // Copy instructions back to the target and destroy the island
    boolean_t ints = ml_set_interrupts_enabled(false);
    disable_write_protection();
    bcopy(island->instructions, island->target, island->insn_bytes);
    OSFree(island, PAGE_SIZE, tag);
    enable_write_protection();
    ml_set_interrupts_enabled(ints);
    
    return KERN_SUCCESS;
}


kern_return_t xnu_unpatch_all(void) {
    while (firstIsland != NULL) {
        kern_return_t err = xnu_unpatch(firstIsland->target);
        if (err != KERN_SUCCESS)
            return err;
    }
    return KERN_SUCCESS;
}
