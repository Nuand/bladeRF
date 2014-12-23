bladeRF Versioning and Tags
================================================================================

This document describes the versioning and tagging scheme that shall be used
when updating the version numbers of various components in the bladeRF project.

--------------------------------------------------------------------------------

Version Numbering Scheme
================================================================================

## Individual Components ##

The version numbering scheme of the following components shall follow the
[Semantic Versioning 2.0.0][semver] scheme in order to convey meaningful
information about changes.

In short, versions shall take the form ```major.minor.patch[-extra]``` and
are used to describe changes to a component's primary interface(s).

* The major version shall be incremented to denote reverse-incompatible
  changes in the component's interface(s).
     * This should be a very infrequent occurrence, and one that is
       met with serious thought and consideration for upgradability.

* The minor version shall be incremented to denote reverse-compatible
  additions.

* The minor version shall be incremented for reverse-compatible bug fixes and
  internal changes that do not affect the primary interface(s).

The ```-extra``` portion will generally be used to include the git revision
associated with the component when it was built.

Below are the versioned components and a brief description of their primary
interface(s).

* bladeRF-cli
  * Command set and command line parameters
* libbladeRF
  * The libbladeRF API
* FX3 Firmware
  * Format and usage of control and bulk transfers, as well as the
    usage of endpoints, interfaces, and alt. settings.
* FPGA
  * FPGA <-> FX3 control and data streaming interfaces


[semver]: http://semver.org/spec/v2.0.0.html


## Project-wide Version ##

The project-wide version number is based upon the release date. The rationale
for this is that the individual components are already versioned; A number
with major, minor, and patch values would not add additional value.

The project-wide version number takes the format ```YYYY.MM[-rcN]``` where:

* ```YYYY``` is a four digit year
* ```MM``` is a two-digit month
* ```-rcN``` is a release-candidate suffix

Items marked with the `-rcN` should be regarded as betas. While more stable
than nightly builds, these may contain new functionality still undergoing
review and testing.

As of Dec 2014, the `YYYY-MM` portions correspond to the date of the release
(candidate).  Prior to this, these values were set to the scheduled release,
which caused confusion and issues with systems trying to sort tags.

For example, an release candidate in Dec 2014 will now be marked
```2014-12-rc1```, whereas it might have been labeled ```2014-02-rc1` in the
past.


Tagging Scheme
================================================================================

The tagging is intended to closely follow the above version number scheme.

## Individual Components ##

Components are individually tagged in order to aid developers in determining
which commits are before/after a particular version number. **Users and
package maintainers will generally only want to pay attention to the
project-wide tags.**

Tags shall be applied in the form, ```<component>_vX.Y.Z```.

Below is the list of ```<component>``` values to use:

* bladeRF-cli
* libbladeRF
* firmware
    * This is for FX3 firmware
* fpga
    * This is for all FPGA and HDL-related changes, including the NIOS II code.


## Project-wide Tags ##

The project-wide tags are the same as the project-wide version number: ```YYYY.MM[-rcN]```
