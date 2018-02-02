//
//  Thunderbolt3Unblocker.c
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 2/1/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

#include <mach/mach_vm.h>
#include <os/log.h>

#include "xnu_override.h"


void _ZN24IOThunderboltSwitchType321shouldSkipEnumerationEv(void);


// Here's a simple set of functions to patch
int patch_me(void) {
    return 42;
}

int (*orig_patch_me)(void) = NULL;
int new_patch_me(void) {
    return 43;
    //return orig_patch_me() + 1;
}


kern_return_t Thunderbolt3Unblocker_start(kmod_info_t *ki, void *d)
{
    kern_return_t err;
    err = xnu_override(patch_me, new_patch_me, (void **)&orig_patch_me);
    if (err != KERN_SUCCESS) {
        os_log(OS_LOG_DEFAULT, "Failed to patch function\n");
        return err;
    }
    
    os_log(OS_LOG_DEFAULT, "Patched function, about to call...\n");
    //os_log(OS_LOG_DEFAULT, "Patched function, return value %d\n", patch_me());
    //os_log(OS_LOG_DEFAULT, "Original function, return value %d\n", orig_patch_me());
    
    return KERN_SUCCESS;
}

kern_return_t Thunderbolt3Unblocker_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}
