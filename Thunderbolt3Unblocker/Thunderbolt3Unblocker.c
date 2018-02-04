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
#include "xnu_override_test.h"


int _ZN24IOThunderboltSwitchType321shouldSkipEnumerationEv(void);
int (*orig_skip_enumeration)(void) = NULL;
int new_skip_enumeration(void) {
    os_log(OS_LOG_DEFAULT, "Thunderbolt3Unblocker: Patched IOThunderboltSwitchType3::shouldSkipEnumeration() called\n");
    return 0;
}


kern_return_t Thunderbolt3Unblocker_start(kmod_info_t *ki, void *d)
{
    // Run a preflight sanity check
    kern_return_t err;
    err = xnu_override_test();
    if (err != KERN_SUCCESS) {
        os_log_error(OS_LOG_DEFAULT, "Thunderbolt3Unblocker: Preflight sanity check failed, aborting\n");
        return err;
    }
    
    // Now patch IOThunderboltSwitchType3::shouldSkipEnumeration()
    err = xnu_override(_ZN24IOThunderboltSwitchType321shouldSkipEnumerationEv, new_skip_enumeration, (void **)&orig_skip_enumeration);
    if (err != KERN_SUCCESS) {
        os_log_error(OS_LOG_DEFAULT, "Thunderbolt3Unblocker: Failed to patch function\n");
        return err;
    }
    
    os_log(OS_LOG_DEFAULT, "Thunderbolt3Unblocker: Patched IOThunderboltFamily\n");
    return KERN_SUCCESS;
}


kern_return_t Thunderbolt3Unblocker_stop(kmod_info_t *ki, void *d)
{
    xnu_unpatch_all();
    return KERN_SUCCESS;
}
