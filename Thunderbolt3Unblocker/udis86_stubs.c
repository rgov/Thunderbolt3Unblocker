//
//  udis86_stubs.c
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 2/2/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

// We cannot build libudis86 out-of-the-box in standalone mode, and if we build
// it in normal mode, it tries to import a few symbols that aren't in xnu. This
// file implements stubs for those symbols.

void *__stdinp = 0;

int fgetc(void) {
    return 0;
}

int __sprintf_chk(void) {
    return 0;
}

int __vsnprintf_chk(void) {
    return 0;
}
