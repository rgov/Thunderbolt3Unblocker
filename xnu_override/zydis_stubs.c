//
//  zydis_stubs.c
//  xnu_override
//
//  Created by Ryan Govostes on 2/15/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

// We cannot link Zydis into kernel because __assert_rtn is not available.
// So in this file we define it.

#include <kern/debug.h>

void __assert_rtn(const char *func, const char *file, int line, const char *message) {
    panic("%s:%d:%s: %s", file, line, func, message);
}
