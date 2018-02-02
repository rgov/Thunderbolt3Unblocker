//
//  xnu_override.c
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 2/2/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

#include <libkern/OSMalloc.h>
#include <os/log.h>
#include <string.h>

#define __UD_STANDALONE__ 1
#include "udis86.h"

#include "xnu_override.h"


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
    // FIXME: Bad! We don't want to screw up %rax on return from our function...
    0x48, 0xA1, 0xFF, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, // mov %rax, ...
    0xFF, 0xE0,                                                 // jmp %rax
};

char kPatchTemplate[] = {
    // Our code to jump to the desired place
    0x48, 0xA1, 0xFF, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, // mov %rax, ...
    0xFF, 0xE0,                                                 // jmp %rax
};

typedef struct {
    unsigned int insn_bytes;
    char instructions[sizeof(kIslandTemplate)];
    // TODO: Make this a linked list, so we can unpatch at unload
} BranchIsland;


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
    os_log(OS_LOG_DEFAULT, "Disabling kernel write protection\n");
    set_cr0(get_cr0() & ~CR0_WP);
}

static void enable_write_protection(void) {
    // Re-enable write protection
    os_log(OS_LOG_DEFAULT, "Re-enabling kernel write protection\n");
    set_cr0(get_cr0() | CR0_WP);
}


kern_return_t xnu_override(void *target, const void *replacement, void **original) {
    if (!tag)
        tag = OSMalloc_Tagalloc("branch_island", OSMT_DEFAULT);
    
    // Allocate a branch island
    os_log(OS_LOG_DEFAULT, "Creating branch island\n");
    
    BranchIsland *island = OSMalloc(sizeof(BranchIsland), tag);
    bcopy(kIslandTemplate, &island->instructions, sizeof(island->instructions));
    
    // Now we will use the disassembler to copy instructions over one by one
    // until we have enough room for our jump
    ud_t u;
    ud_init(&u);
    ud_set_input_buffer(&u, target, 64 /* assume >64 bytes */);
    ud_set_mode(&u, 64);
  
    island->insn_bytes = 0;
    void *insn_ptr = &island->instructions;
    while (island->insn_bytes < sizeof(kPatchTemplate)) {
        if (!ud_disassemble(&u)) {
            os_log_error(OS_LOG_DEFAULT, "Cannot disassemble instruction, aborting\n");
            goto fail;
        }
        
        island->insn_bytes += ud_insn_len(&u);
        if (island->insn_bytes > kOriginalInstructionsSize) {
            os_log_error(OS_LOG_DEFAULT, "Out of space in branch island, aborting\n");
            goto fail;
        }
        
        bcopy((void *)ud_insn_ptr(&u), insn_ptr, ud_insn_len(&u));
        insn_ptr += ud_insn_len(&u);
    }
    
    // Ok, the branch island is almost ready, we just need to insert the address
    // we want it to jump to, which will be the first instruction after the ones
    // we copied into the island.
    uint64_t jumpTo = (uint64_t)target + island->insn_bytes;
    bcopy(&jumpTo, &island->instructions[kIslandJumpOffset], sizeof(uint64_t));

    // Now we need to patch the target to insert a jump to the replacement code
    disable_write_protection();
    jumpTo = (uint64_t)replacement;
    bcopy(kPatchTemplate, target, sizeof(kPatchTemplate));
    bcopy(&jumpTo, target + kPatchJumpOffset, sizeof(uint64_t));
    enable_write_protection();
    
    // Return a pointer to the branch island, which is how to call the original
    // function.
    *original = &island->instructions;
    
    os_log(OS_LOG_DEFAULT, "Patch applied\n");
    return KERN_SUCCESS;
    
fail:
    OSFree(island, sizeof(BranchIsland), tag);
    return KERN_ABORTED;
}


kern_return_t xnu_unpatch(void *target, const void *original) {
    os_log_error(OS_LOG_DEFAULT, "Reverting patch\n");
    
    // Find the BranchIsland
    BranchIsland *island = (void *)original - offsetof(BranchIsland, instructions);
    
    // Just copy instructions back from the island
    disable_write_protection();
    bcopy(island->instructions, target, island->insn_bytes);
    enable_write_protection();
    
    return KERN_SUCCESS;
}
