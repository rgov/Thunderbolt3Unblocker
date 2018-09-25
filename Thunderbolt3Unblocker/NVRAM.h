//
//  NVRAM.h
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 9/25/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

// NB: I am not sure that this implementation is entirely correct. I'm basically
// copying the code from xnu's IOPlatformExpert.cpp. You should declare a
// dependency on com.apple.driver.AppleEFINVRAM.

#ifndef NVRAM_h
#define NVRAM_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int readNVRAMProperty(const char *symbol, void *value, size_t *len);
int writeNVRAMProperty(const char *symbol, const void *value, size_t len);
int deleteNVRAMProperty(const char *symbol);

#ifdef __cplusplus
}
#endif

#endif /* NVRAM_h */
