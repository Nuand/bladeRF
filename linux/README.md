# bladeRF Linux Source #
The linux source is separated out into three separate areas:

| Directory         | Description                                                                                                                                           |
| :-----------------| :---------------------------------------------------------------------------------------------------------------------------------------------------- |
| [apps][apps]      | A collection of applications written to show how to interact with libbladeRF and provide a command-line interface for interacting with the hardware   |
| [kernel][kernel]  | Kernel driver source code which interacts with Linux's USB subsystem                                                                                  |
| [lib][lib]        | The source for libbladeRF which interacts with the kernel and provides a C-callable interface for opening, probing and interacting with the device    |

[apps]: apps (Linux Applications)
[kernel]: kernel (Linux Kernel Driver)
[lib]: lib (libbladeRF Source)

It's highly recommended to build the linux directories in order:

1. [kernel][kernel]
1. [lib][lib]
1. [apps][apps]

