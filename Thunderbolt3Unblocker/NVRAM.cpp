//
//  NVRAM.cpp
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 9/25/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

#include "NVRAM.h"

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IONVRAM.h>
#include <IOKit/IORegistryEntry.h>

#include <os/log.h>

extern "C" {

static IODTNVRAM *getNVRAMEntry(void) {
    IORegistryEntry *entry = IORegistryEntry::fromPath("/options", gIODTPlane);
    if (!entry) {
        os_log_error(OS_LOG_DEFAULT, "Failed to get NVRAM entry!\n");
        return NULL;
    }
    
    IODTNVRAM *nvram = OSDynamicCast(IODTNVRAM, entry);
    if (!nvram) {
        entry->release();
        os_log_error(OS_LOG_DEFAULT, "Failed to cast to IODTNVRAM instance!\n");
        return NULL;
    }
    
    return nvram;
}


int readNVRAMProperty(const char *symbol, void *value, size_t *len) {
    
    if (!symbol || !len)
        return 0;
    
    IODTNVRAM *nvram = getNVRAMEntry();
    if (!nvram)
        return 0;
    
    // Save the input buffer length for later
    size_t vlen = *len;
    *len = 0;
    
    OSObject *obj = nvram->getProperty(symbol);
    if (!obj) {
        os_log_debug(OS_LOG_DEFAULT, "Could not get NVRAM property %s\n",
                     symbol);
        nvram->release();
        return 0;
    }
    
    // Try to convert the object to an OSData or OSString
    if (OSData *data = OSDynamicCast(OSData, obj)) {
        *len = (size_t)data->getLength();
        
        // Without a value, just return the length
        if (!value) {
            nvram->release();
            return 1;
        }
        
        if (*len < vlen)
            vlen = *len;
        memcpy((void *)value, data->getBytesNoCopy(), vlen);
    } else if (OSString *string = OSDynamicCast(OSString, obj)) {
        *len = (size_t)string->getLength() + 1;
        
        // Without a value, just return the length
        if (!value) {
            nvram->release();
            return 1;
        }
        
        if (*len < vlen)
            vlen = *len;
        memcpy((void *)value, string->getCStringNoCopy(), vlen);
    } else {
        os_log_error(OS_LOG_DEFAULT, "Unexpected type in NVRAM property: %s\n",
                     obj->getMetaClass()->getClassName());
        nvram->release();
        return 0;
    }
    
    nvram->release();
    return 1;
}
    
int writeNVRAMProperty(const char *symbol, const void *value, size_t len) {
    if (!symbol || !value)
        return 0;
    
    // Get the symbol as an OSSymbol
    const OSSymbol *sym = OSSymbol::withCStringNoCopy(symbol);
    if (!sym) {
        return 0;
    }
    
    // Get the data as an OSData
    OSData *data = OSData::withBytes((void *)value, len);
    if (!data) {
        sym->release();
        return 0;
    }
    
    // Get the NVRAM registry entry
    IODTNVRAM *nvram = getNVRAMEntry();
    if (!nvram) {
        sym->release();
        data->release();
        return 0;
    }
    
    // Write to NVRAM
    int ret = nvram->setProperty(sym, data);
    nvram->sync();
    nvram->release();
    data->release();
    sym->release();
    return ret;
}

int deleteNVRAMProperty(const char *symbol) {
    if (!symbol)
        return 0;
    
    // Get the symbol as an OSSymbol
    const OSSymbol *sym = OSSymbol::withCStringNoCopy(symbol);
    if (!sym) {
        return 0;
    }
    
    // Get the NVRAM registry entry
    IODTNVRAM *nvram = getNVRAMEntry();
    if (!nvram) {
        sym->release();
        return 0;
    }
    
    // Remove the symbol
    nvram->removeProperty(sym);
    nvram->sync();
    nvram->release();
    sym->release();
    return 1;
}

}
