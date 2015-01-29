bladeRF Package Maintainer's Guide
================================================================================

This document is intended to provide guidance and recommendations to bladeRF
package maintainers, or anyone redistributing bladeRF binaries.

Feedback on this document is greatly appreciated, as it will help establish
common practices and procedures.

## Objectives ##

Below are the objectives for packaging, from the bladeRF development team's
perspective.

Package maintainers should not have to jump through hoops to package bladeRF
support in manner required by their OS's package repositories. Specifically,
package maintainers **should not** have to apply patches to build and deploy
bladeRF support. More importantly, the end users should not need to perform
any excessive configuration.

The bladeRF project's CMake flags should provide the ability to sufficiently
change paths, build options, and configure the use of optional dependencies. If
this is found **not** to be the case, it should be considered a defect in the
bladeRF project and [reported as a bug] on the [issue tracker].

[reported as a bug]: reporting_bugs.md
[issue tracker]: https://github.com/Nuand/bladeRF/issues

[COPYING]: ../COPYING


## Components ##

The bladeRF repository consists of the following components:

#### libbladeRF ####

The libbladeRF host library provides the ability to configure and operate the
device. This is the primary item to distribute in binary packages.

Required dependencies:

* pthreads
* libusb-1.0.
    * For best results with USB 3.0, a very recent version is recommended.


Optional build dependencies:

* [Doxygen](http://www.doxygen.org)
    * This allows libbladeRF API documentation to be built.


#### bladeRF-cli ####

This program provides a command-line interface atop of libbladeRF.

It is **highly** recommended that this be provided to users, as it is provides
the ability to upgrade the device, as well as a number of commands to quickly
test and verify device operation.

Required dependencies:

* libbladeRF
* pthreads
* libusb-1.0
* libm (math)

Optional dependencies:

* [libtecla](http://www.astro.caltech.edu/~mcs/tecla/)
    * This is recommended, as it provides history, tab-completion, and
      vi or emacs key-bindings to the bladeRF-cli program, yielding a much
      improved user experience.

Optional build dependencies:

* [pandoc](http://johnmacfarlane.net/pandoc/)
    * This is used to build the bladeRF-cli help text. This is
      recommended if your package is tracking "bleeding edge" changes.
      However, it should not be necessary when tracking release tags, as the
      pre-built help text should have been updated as part of the release
      procedures.

* [help2man](https://www.gnu.org/software/help2man/)
    * This is used to generate the bladeRF-cli manual page.


#### FX3 Firmware ####

This firmware runs on the Cypress FX3 USB controller and performs the following:

* Loading the FPGA
* Handling SPI flash access
* Dispatching device configuration requests to the FPGA
* Performing DMA operations associated with sample streaming.

Once written into SPI flash (via a libbladeRF API call or the bladeRF-cli),
the device will boot this firmware automatically.

This firmware requires the [Cypress FX3 SDK](http://www.cypress.com/?rID=57990)
to build. As such, it is recommended that this firmware be obtained via the
Nuand-hosted pre-built binaries, linked later in this document.

The firmware does not need to be stored in any particular location. It simply
needs to be user-accessible for upgrades.

#### FPGA bitstream ####

The FPGA controls and interfaces with the LMS6002D transceiver and the SI5338
clock generator.  There are two bladeRF variants, each with a different sized
FPGA.  These differences are denoted by the FPGA variant name, ```x40``` or
```x115```, with the latter being the larger FPGA.  The ```x40``` and
```x115``` require different FPGA images.

These FPGA bitstreams must be loaded onto the device every time it is powered
on. To alleviate the need to manually do this there are two [autoloading]
mechanisms:

* [Host-based autoloading]
    * By storing the FPGA bitstreams (named ```hostedx40.rbf``` or
      ```hostedx115.rbf```) in particular locations, libbladeRF can
      find and load the appropriate FPGA image when opening a device handle.
      See the above link for more information about the searched locations.
      **This is the recommended autoloading option.**

* Flash-based autoloading
    * This is recommended only for advanced users with a need for the FPGA to
      be autoloaded without a host machine. The FPGA image can be written to
      SPI flash via the bladeRF-cli program.

It is recommended that the ```x40``` and ```x115``` be shipped in the
directories associated with host-based autoloading.

The [Altera Quartus II] tools are required to build these FPGA bitstreams. It is
recommended that you use Nuand's pre-built images, linked later in this
document.

[autoloading]: https://github.com/Nuand/bladeRF/wiki/FPGA-Autoloading
[Host-based autoloading]: https://github.com/Nuand/bladeRF/wiki/FPGA-Autoloading#host-software-based
[Altera Quartus II]: http://dl.altera.com/13.1/?edition=web

## Configuration and Build ##

This section briefly notes some important CMake configuration flags. Please
see the README files in the source tree for more complete information about
available CMake options.

#### Release Build ####

Use ```-DCMAKE_BUILD_TYPE=Release``` to configure a release build without
debug symbols.

Use ```-DCMAKE_BUILD_TYPE=RelWithDebInfo``` to configure a release build
**with** debug symbols.

#### Extra Version Information ####

Normally, a git revision will be appended to version numbers. To remove this,
run CMake with ```-DTAGGED_RELEASE=Yes``` when building from the top-level, or
```-DVERSION_INFO_EXTRA=""``` otherwise.

If you would like to append package information to version numbers, you may use
the aforementioned ```VERSION_INFO_OVERRIDE``` option. For example:
    ```-DVERSION_INFO_OVERRIDE="buildbot-2014.11.30"```

If you forget to clear the "extra" version info field, you will wind up with
a Git changeset appended to components' version strings, or ```-git-unknown```
if a git repository was not found.


#### Documentation ####

Use ```-DBUILD_DOCUMENTATION=ON``` to enable the generation of documentation.

When necessary tools (Doxygen, pandoc, help2man) are found, the above
configuration option will default the libbladeRF API docs and the bladeRF-cli
help text and manpage to be ON.

See the libbladeRF and bladeRF-cli README files for more information about
enabling/disabling documentation generation.

## Versions and Tags ##

As noted in [versioning] document, package maintainers should only be concerned
with the project-wide releases and tags, which are in the following format:

    YYYY.MM[-rcN]

For example: ```2014.11``` or ```2014.11-rc3``` (for release candidate 3).

To view these tags in the repository:
```
$ git tag | grep '^20[0-9]\{2\}\.[0-9]\{2\}\(-rc[0-9]\+\)\?'
```

The top-level [CHANGELOG] file lists the various component versions that are
associated with a tagged release with note similar to the following:
```
This release candidate consists of the following versions:
  * FPGA bitstream v0.1.2
  * FX3 firmware v1.8.0
  * libbladeRF v1.0.0
  * bladeRF-cli v1.0.0
```


[versioning]: development/versioning.md
[CHANGELOG]: ../CHANGELOG

## Pre-built Binaries ##
Official pre-built [FX3 firmware images] and [FPGA bitstreams] are hosted
on the Nuand website, along with MD5 and SHA256 checksums.

Whether or not these binaries may be included in packages will likely vary with
OS/Distro policies.

Be aware that while that Nuand's HDL files and FX3 source code are MIT
licensed, the required Altera tools and Cypress libraries required to build
these items are not open source; this is unfortunately unavoidable.

[FX3 firmware images]: https://www.nuand.com/fx3.php
[FPGA bitstreams]: https://www.nuand.com/fpga.php

## Licenses ##

The top-level [COPYING] file notes the licenses associated with each component
in the project.
