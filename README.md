# Thunderbolt 3 Unblocker

This project provides a kernel extension that unblocks unsupported Thunderbolt
3 peripherals (such as the Razer Core) on macOS.

This accomplishes the same goal as [KhaosT's TB3 Enabler][tb3-enabler], which
works by patching IOThunderboltFamily on disk. This kernel extension performs
the patch in memory and on-the-fly.

[tb3-enabler]: https://github.com/KhaosT/tb3-enabler

Note there is likely a reason why IOThunderboltFamily considers a peripheral
unsupported in the first place. Use at your own peril.

## Installation

To prepare your development environment, please run

    git submodule update --init --recursive
    brew install cmake

It is recommended to keep System Integrity Protection on. To lower security
just enough to allow this kernel extension to be loaded without code signing, 
use `csrutil enable --without kext`

Build the project with Xcode. Make sure to change code signing settings as
appropriate.

    sudo chown -R root:wheel Thunderbolt3Unblocker.kext
    sudo kextload Thunderbolt3Unblocker.kext


## `xnu_override`

This project also implements a simple, reusable in-memory kernel patching
library. The author has released it under a permissive license in the hopes
that it will be useful.
