# bladeRF Linux Kernel Driver #
The linux kernel driver implements a USB device and has some buffering to be able to place multiple packets in flight at a time achieve the high datarates associated with USB 3.0 Superspeed.

## Requirements ##
Even though we build a module for the kernel, your kernels source code is required to build against.  We've tested with kernels as old as 3.5.0, but we mostly use Ubuntu for testing and development.

## Building ##
1. `make`
1. See that `bladeRF.ko` has been made.
1. `sudo insmod bladeRF.ko`

The kernel module should now be installed and ready to be used.

## bladeRF udev Rule ##
Usually the udev rules associated with USB devices use the VID and PID to determine the matching for the rule and change the mode to be accessible to users of that group.  In our testing, it's been seen that the particular udev rule changes the mode of `/dev/bus/usb/...` but not the preferred dev entry we create of `/dev/bladerf#`.  Due to this, we have a relatively broad udev rule:

```
KERNEL=="bladerf*", MODE="0660", GROUP="plugdev"
```

Please feel free to suggest a tighter rule that applies the appropriate mode to the `/dev/bladerf#` entry.

