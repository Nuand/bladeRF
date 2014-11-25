bladeRF Bug Reporting Guide
================================================================================

A common question received by bladeRF developers is, "How can I help?" This is
a great question to hear, and one with a myriad of answers.

One of the top answers is simply to report bugs!

More specifically -- report bugs with plenty of information and analysis to
cut down on the time required for investigation and debugging. (Even better,
include some potential solutions or patches!)

This document outlines the preferred bug reporting process and provides some
tips to make bug reports helpful and efficient to handle.

--------------------------------------------------------------------------------

## Verify your version information  ##

When reporting issues, it is very important to gather information pertaining
to the versions of the various components you are using:
 * bladeRF-cli
 * libbladeRF
 * [FX3 Firmware](https://www.nuand.com/fx3)
 * [FPGA bitstream](https://www.nuand.com/fpga)

This information can be obtained through libbladeRF API calls, or via the
```version``` command in the bladeRF-cli program.

Updating to the latest available versions of the above components is generally
the first course of action when encountering issues, as the issue may have
already been resolved.

If you indicate that you are using older versions in a bug report, you will
generally be asked to update first. Do not be offended by this request -- it
simply helps ensure that everyone is on the same page when debugging, as
developers are often working with the "bleeding edge" codebase.

If you are building the above components from git, be sure to include the
git changeset you are building from. Usually, this will be included in the
version string.

Lastly, be sure to note where the binaries you are using are coming from:
 * Build from source
 * Package manager (Distro repository, Ubuntu PPA, MacPorts or Homebrew, etc.)
 * The Windows installer


## Gather detailed technical information ##

The better the quality of information you are able to gather and provide,
the better the chances are that the issue will get resolved quickly.

Make note of the device settings you are using.

Try to provide the exact series of actions required to reproduce the issue.
Make note of any observables or symptoms that lead up to the problem you are
experiencing.

If the sequence of actions to needed to reproduce the failure is fairly
complex, try to provide a script or small program.

If the bug is occurring within the context of a larger program, try to isolate
it to a particular code snippet or try to reproduce the bug in a small program
that you can share -- the more concise the program, the better.

For crashes, especially intermittent ones, it is **very** helpful to come
forth with logs from a debugger. For example, if you find your program to
be segfaulting, a backtrace is very helpful in determining where the program
crashed. The state of variables in various stack frames leading up to the
crash are also important to note. See [this wiki page][debugging] for
tips on gathering this type of intormation.

[debugging]: https://github.com/Nuand/bladeRF/wiki/Debugging


For memory-related defects, [Valgrind](http://valgrind.org/) output is
extremely helpful to gather and provide.


## The bladeRF issue tracker ##

Bugs and feature requests are tracked via the GitHub
[issue tracker](https://github.com/Nuand/bladeRF/issues). This issue tracker
should **not** be used for general support requests. Such requests will be
immediately closed, and you will be asked to move your
questions to the [Nuand forums](https://www.nuand.com/forums).

Again, try to ensure you are running the latest available code before
submitting new issue.

Check for any similar open or closed issues. If there is a relevant
open issue, you should use the existing issue to supply more information.

If you find that there is a closed issue pertaining to your issue, and you are
**positive** that you are running code that includes the changes associated
with the resolution, make note of the closed issue and be sure to include
a reference to it in your bug report. Also include any references to issues
that you suspect to be related or similar.

When referencing issues, use the syntax ```#<issue number>```. GitHub
will automatically create a link for you! For example, ```#312``` will generate
a link to https://github.com/Nuand/bladeRF/issues/312.

When submitting a [new issue](https://github.com/Nuand/bladeRF/issues/new),
please try to use the following convention for the issue title:

```[component] Brief issue description```

Where ```component``` refers to libbladeRF, bladeRF-cli, hdl (for FPGA issues),
or fx3 (for FX3 firmware issues).

If you are not certain which component is at fault, you can leave this out.
A developer will add it when this has been determined.

GitHub will show the first 90 or so characters if the issue title, so try to
make your title concise.

Include all of that excellent information you have gathered in the description.
Try to keep this description self-contained by attaching images or including
logs inline. See the [GitHub Markdown syntax][markdown] for more information on
how to format your post nicely. Specifically, see the *Syntax highlighting*
section for including logs.

[markdown]: https://guides.github.com/features/mastering-markdown/

Remember, you are presenting this information to someone who is not sitting
there with you during your investigation. Make sure you provide ample context!
