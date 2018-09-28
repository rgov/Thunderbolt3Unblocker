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
} BranchIsland;

static OSMallocTag tag = NULL;


// All branch islands will be allocated here. On Mojave, we lost the ability to
// execute from our OSMalloc allocations.
const int maxIslands = 32;
uint8_t islandHeapUsed[maxIslands];
BranchIsland islandHeap[maxIslands] __attribute__((section("__TEXT,__text")));


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


// Allocate a temporary, non-executable island for writing
static BranchIsland *alloc_nx_island(void) {
    if (!tag)
        tag = OSMalloc_Tagalloc("branch_island", OSMT_DEFAULT);
    return OSMalloc(sizeof(BranchIsland), tag);
}

// Move a temporary island to our executable heap
static BranchIsland *move_island(BranchIsland *nx) {
    for (int i = 0; i < maxIslands; i ++) {
        if (islandHeapUsed[i])
            continue;
        
        boolean_t ints = ml_set_interrupts_enabled(false);
        disable_write_protection();
        bcopy(nx, &islandHeap[i], sizeof(BranchIsland));
        enable_write_protection();
        ml_set_interrupts_enabled(ints);
        
        islandHeapUsed[i] = 1;
        return &islandHeap[i];
    }
    
    os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Cannot allocate another ");
    return NULL;
}

static void free_island(BranchIsland *island) {
    long i = island - &islandHeap[0];
    islandHeapUsed[i] = 0;
}


kern_return_t xnu_override(void *target, const void *replacement, void **original) {
    // Allocate a branch island
    os_log_debug(OS_LOG_DEFAULT, LOG_PREFIX "Creating branch island\n");
    
    BranchIsland *island = alloc_nx_island();
    island->target = target;
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
        
        // Detect whether the instruction touches %rip
        bool accessesInstPtr = false;
        for (int oi = 0; oi < instruction.operandCount; oi ++) {
            /*
             Due to a bug currently in Zydis, I cannot do
                ZydisRegisterGetClass(x) == ZYDIS_REGCLASS_IP
             which is a little bit nicer than comparing directly to RIP.
             */
            ZydisDecodedOperand *op = &instruction.operands[oi];
            if (op->type == ZYDIS_OPERAND_TYPE_REGISTER) {
                if (op->reg.value == ZYDIS_REGISTER_RIP) {
                    accessesInstPtr = true;
                    break;
                }
            } else if (op->type == ZYDIS_OPERAND_TYPE_MEMORY) {
                if (op->mem.base == ZYDIS_REGISTER_RIP || op->mem.index == ZYDIS_REGISTER_RIP) {
                    accessesInstPtr = true;
                    break;
                }
            }
        }
        
        if (accessesInstPtr) {
            os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Encountered instruction that accesses instruction pointer, proceed with caution\n");
            if (original != NULL) {
                os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Calling original function will likely crash!\n");
            }
        }
        
        // Make sure we have enough room to proceed
        island->insn_bytes += instruction.length;
        if (island->insn_bytes > kOriginalInstructionsSize) {
            os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Out of space in branch island, aborting\n");
            goto fail;
        }
        
        bcopy(instruction.data, insn_ptr, instruction.length);
        insn_ptr += instruction.length;
    }
    
    // Ok, the branch island is almost ready, we just need to insert the address
    // we want it to jump to, which will be the first instruction after the ones
    // we copied into the island.
    uint64_t jumpTo = (uint64_t)target + island->insn_bytes;
    bcopy(&jumpTo, &island->instructions[kIslandJumpOffset], sizeof(uint64_t));
    
    // Move the island to the executable heap
    island = move_island(island);
    
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
    if (original != NULL) {
        *original = &island->instructions;
    }
    
    os_log(OS_LOG_DEFAULT, LOG_PREFIX "Patch applied\n");
    return KERN_SUCCESS;
    
fail:
    free_island(island);
    return KERN_ABORTED;
}


kern_return_t xnu_unpatch(const void *target) {
    os_log(OS_LOG_DEFAULT, LOG_PREFIX "Reverting patch\n");
    
    // Scan through the list of branch islands for this target
    BranchIsland *island = NULL;
    for (int i = 0; i < maxIslands; i ++) {
        if (islandHeapUsed[i] && islandHeap[i].target == target) {
            island = &islandHeap[i];
            break;
        }
    }
    
    if (!island) {
        os_log_error(OS_LOG_DEFAULT, LOG_PREFIX "Unpatch target not found\n");
        return KERN_ABORTED;
    }
    
    // Copy instructions back to the target and destroy the island
    boolean_t ints = ml_set_interrupts_enabled(false);
    disable_write_protection();
    bcopy(island->instructions, island->target, island->insn_bytes);
    free_island(island);
    enable_write_protection();
    ml_set_interrupts_enabled(ints);
    
    return KERN_SUCCESS;
}


kern_return_t xnu_unpatch_all(void) {
    for (int i = 0; i < maxIslands; i ++) {
        if (islandHeapUsed[i]) {
            kern_return_t err = xnu_unpatch(islandHeap[i].target);
            if (err != KERN_SUCCESS)
                return err;
            
            free_island(&islandHeap[i]);
        }
    }
    return KERN_SUCCESS;
}
