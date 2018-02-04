//
//  xnu_override.h
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 2/2/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

#ifndef xnu_override_h
#define xnu_override_h

#include <mach/kern_return.h>

kern_return_t xnu_override(void *target, const void *replacement, void **original);
kern_return_t xnu_unpatch(const void *target);
kern_return_t xnu_unpatch_all(void);

#endif /* xnu_override_h */
