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
int patch_me(int n __unused) {
    // Our patch target cannot be too small, so we have to do some stuff.
    int a = 1, b = 1;
    for (int i = 0; i < n; i ++) {
        int tmp = a;
        a = b;
        b += tmp;
    }
    return a;
}

int (*orig_patch_me)(int) = NULL;
int new_patch_me(int n) {
    return 42;
}


kern_return_t Thunderbolt3Unblocker_start(kmod_info_t *ki, void *d)
{
    volatile int n = 100;
    os_log(OS_LOG_DEFAULT, "Before patch, return value %d\n", patch_me(n));
    
    kern_return_t err;
    err = xnu_override(patch_me, new_patch_me, (void **)&orig_patch_me);
    if (err != KERN_SUCCESS) {
        os_log(OS_LOG_DEFAULT, "Failed to patch function\n");
        return err;
    }
    
    os_log(OS_LOG_DEFAULT, "Patched function, about to call...\n");
    //os_log(OS_LOG_DEFAULT, "Patched function, return value %d\n", patch_me(n));
    //os_log(OS_LOG_DEFAULT, "Original function, return value %d\n", orig_patch_me(n));
    
    return KERN_SUCCESS;
}

kern_return_t Thunderbolt3Unblocker_stop(kmod_info_t *ki, void *d)
{
    // Revert the patch if it is active
    if (orig_patch_me != NULL) {
        os_log(OS_LOG_DEFAULT, "Active patch detected at unload, reverting\n");
        xnu_unpatch(patch_me, orig_patch_me);
    }
    
    return KERN_SUCCESS;
}
