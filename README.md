[![Build Status](https://travis-ci.org/rgov/Thunderbolt3Unblocker.svg?branch=master)](https://travis-ci.org/rgov/Thunderbolt3Unblocker)

| :warning: Thunderbolt3Unblocker does not yet work on Apple Silicon. |
|---------------------------------------------------------------------|

# Thunderbolt 3 Unblocker

This project provides a kernel extension that unblocks unsupported Thunderbolt
3 peripherals (such as the Razer Core) on macOS.

This accomplishes the same goal as [KhaosT's TB3 Enabler][tb3-enabler], which
works by patching IOThunderboltFamily on disk. This kernel extension performs
the patch in memory and on-the-fly.

[tb3-enabler]: https://github.com/KhaosT/tb3-enabler

Note there is likely a reason why IOThunderboltFamily considers a peripheral
unsupported in the first place. Use at your own peril.

This kernel extension has been tested against macOS Monterey 12 and as far
back as macOS Sierra 10.12.6. Please check for [open issues][issues] before
using on other versions, and review the [troubleshooting guide][trouble].

[issues]: https://github.com/rgov/Thunderbolt3Unblocker/issues
[trouble]: https://github.com/rgov/Thunderbolt3Unblocker/wiki/Troubleshooting


## Installation

Please head over to the [Releases][] page for binaries and installation
instructions.

[Releases]: https://github.com/rgov/Thunderbolt3Unblocker/releases


## Building

To prepare your development environment, please run

    git submodule update --init --recursive
    brew install cmake

Build the project with Xcode. Make sure to change code signing settings as
appropriate.

Load the kernel extension with:

    sudo chown -R root:wheel Thunderbolt3Unblocker.kext
    sudo kextload Thunderbolt3Unblocker.kext

If loading the kext fails: Reboot into Recovery Mode and disable kext security
restrictions using `csrutil enable --without kext`.

If you are developing the kext, you should know that the NVRAM variable `t3u-incompatible` is written whenever there is a panic while loading the kext. The presence of this variable prevents the kext from loading again on the same system version. You may want to disable the code that does this (in Thunderbolt3Unblocker.c), or delete it with `nvram -d t3u-incompatible`.


## `xnu_override`

This project also implements a simple, reusable in-memory kernel patching
library. The author has released it under a permissive license in the hopes
that it will be useful.
