//
//  Thunderbolt3Unblocker.c
//  Thunderbolt3Unblocker
//
//  Created by Ryan Govostes on 2/1/18.
//  Copyright © 2018 Ryan Govostes. All rights reserved.
//

#include <mach/mach_types.h>

kern_return_t Thunderbolt3Unblocker_start(kmod_info_t * ki, void *d);
kern_return_t Thunderbolt3Unblocker_stop(kmod_info_t *ki, void *d);

kern_return_t Thunderbolt3Unblocker_start(kmod_info_t * ki, void *d)
{
    return KERN_SUCCESS;
}

kern_return_t Thunderbolt3Unblocker_stop(kmod_info_t *ki, void *d)
{
    return KERN_SUCCESS;
}
