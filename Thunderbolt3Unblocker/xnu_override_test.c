//
//  xnu_override.c
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 2/4/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//


#include <os/log.h>
#include <string.h>

#include "xnu_override.h"


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

kern_return_t xnu_override_test(void) {
    // Capture the original return value
    volatile int n = 40;
    int orig_return_value = patch_me(n);
    
    // Capture the return value from the replacement function
    int new_return_value = new_patch_me(n);
    
    // Attempt to patch the function
    kern_return_t err;
    err = xnu_override(patch_me, new_patch_me, (void **)&orig_patch_me);
    if (err != KERN_SUCCESS) {
        os_log_error(OS_LOG_DEFAULT, "xnu_override_test: Failed to patch function\n");
        return err;
    }
    
    // Make sure the patched function returns what we expected
    if (patch_me(n) != new_return_value) {
        os_log_error(OS_LOG_DEFAULT, "xnu_override_test: Unexpected return from patched function\n");
        return KERN_ABORTED;
    }
    
    // Make sure we can still call the original function
    if (orig_patch_me(n) != orig_return_value) {
        os_log_error(OS_LOG_DEFAULT, "xnu_override_test: Unexpected return from original function\n");
        return KERN_ABORTED;
    }
    
    // Unpatch
    err = xnu_unpatch(patch_me);
    if (err != KERN_SUCCESS) {
        os_log_error(OS_LOG_DEFAULT, "xnu_override_test: Failed to unpatch function\n");
        return KERN_ABORTED;
    }
    
    // Make sure the function works as it used to
    if (patch_me(n) != orig_return_value) {
        os_log_error(OS_LOG_DEFAULT, "xnu_override_test: Unexpected return from unpatched function\n");
        return KERN_ABORTED;
    }
    
    // Make sure we can't unpatch again
    err = xnu_unpatch(patch_me);
    if (err == KERN_SUCCESS) {
        os_log_error(OS_LOG_DEFAULT, "xnu_override_test: Unpatch safety checks failed\n");
        return KERN_ABORTED;
    }
    
    return KERN_SUCCESS;
}
