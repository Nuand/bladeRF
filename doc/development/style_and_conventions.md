bladeRF Style and Coding Conventions
================================================================================

This document describes the preferred conventions and coding style for the
various components of the bladeRF project.

Although style is generally a matter of preference and many of the items
outlined here are purely aesthetic, the goal of these guidelines is to
encourage consistency within the codebase as new changes are introduced.

Other guidelines presented here are intended to ensure portability, avoid bugs,
and increase debug-ability. Please follow these guidelines when submitting new
changes or code.

The current code does not necessarily follow these desired conventions.
However, over time, it should migrate towards being compliant with these
guidelines. Patches are always welcome, but please separate style/convention
changes from functional changes, when possible.


_When in doubt, err towards readability and verbosity._

--------------------------------------------------------------------------------




C Code
================================================================================

Below are the preferred style and conventions for C source code. This generally
applies to host-side code.  However, keep these items in mind for the embedded
code. As the embedded code is tightly coupled with APIs that deviate from the
preferred style, favor consistency where obvious discrepancies exist.


Indentation and Whitespace
--------------------------------------------------------------------------------

 * Spaces only, no tabs.
 * Indents shall be 4 spaces.
 * Use Unix ("\n") line endings (as opposed to DOS "\r\n" line endings)
 * No trailing whitespace at the end of lines.
   * Do not indent on lines used for vertical whitespace.


## Naming (Variables, Functions, etc.) ##

### Variables ###
Use lowercase characters and underscores. Short and simple names are fine for
scopes with only a few variables, or when the variable name is clearly
unambiguous.  Use more descriptive names as scope and complexity increases.

For example, `struct mystate s;` is fine if there is clearly only one state
variable in the scope.

However, if a function is converting samples to bytes and contains number of
variables tracking various different counts, a `samples_per_buffer`
variable might be more appropriate than `num` or `num_samples`.


### Functions ###
Use lowercase characters and underscores. When applicable, try to name
functions in the following format.

`[module]_[submodule]_[verb]_[adjective]_[noun]`

The <module> and <submodule> portions are generally unnecessary for static
helper functions.

Examples:

`
int lms_get_dc_cals(struct bladerf *dev, struct bladerf_lms_dc_cals *dc_cals);
`

`
int flash_write_fpga_bitstream(struct bladerf *dev, uint8_t **bitstream,
                               size_t len);
`



### Macros ###
Use uppercase characters and underscores. See the above functions rules
for function-like macros.

### Explicit Conditionals ###

Explicitness is preferred in conditionals. For example,

```if (ptr != NULL)``` is preferred over ```if (ptr)```

```if ((value & (1 << 0)) != 0)``` is preferred over ```if (value & (1 << 0))```.


### Formatting ###

#### Long lines ####

When possible, restrict lines to 80 characters or less. This is intended to:

 * Ensure that developers can use vertical splits in editors without line
   wrapping on them

 * Indicate that code is becoming too nested

 * Indicate that conditionals might be simplified

If function prototypes are long, break the line along arguments. You may wish
to group related arguments on the same line. The following
are both acceptable:

<pre>
int bladerf_set_sample_rate(struct bladerf *dev, bladerf_module module,
                            unsigned int rate, unsigned int *actual);
</pre>


<pre>
int bladerf_set_sample_rate(struct bladerf *dev,
                            bladerf_module module,
                            unsigned int rate,
                            unsigned int *actual);
</pre>


When conditionals become long, first consider if there is a simpler or better
way to organize the logic. Next, consider assigning some ```const bool```
variables for any complicated conditions.

If attempting to break long lines simply creates a mess, err toward readability
and leave it long. However, ensure you have considered the above comments.


#### Braces, Parens, Spacing ####

With respect to placement of braces and parenthesis, the below example is
intended to exemplify the desired style.

**Always** use braces with if and while statements, even they only contain a
single statement. This is intended to avoid mistakes when other developers
modify the code, add debug statements, etc.

-------------------------------------------------------------------------------
```c
int mymodule_perform_device_operation(struct bladerf *dev,
                                      const uint8_t *input_array,
                                      uint8_t **output_array,
                                      size_t num_entries)
{
    unsigned int i;
    uint8_t new_value;
    uint32_t flags;
    uint8_t *results;

    int status = 0;
    bool done = false;

    results = calloc(num_entries, sizeof(input_array[0]));
    if (results == NULL) {
        *output_array = NULL;
        return BLADERF_ERR_MEM;
    }

    for (i = 0; i < num_entries; i++) {
        new_value = (input_array[i] >> 4) | ((input_array[i] & 0x9) << 4);

        switch (input_array[i]) {
            case 0x00:
            case 0x01:
                flags = NORMAL_OPTIONS | SPECIAL_OPTIONS;
                break;

            case 0x80:
                flags = NORMAL_OPTIONS;
                break;

            case 0xff:
                flags = DEBUG_OPTIONS;
                break;

            default:
                status = BLADERF_ERR_INVAL;
                log_debug("Invalid value: 0x%02X at idx=%u\n",
                          input_array[i], i);
                goto out;
        }

        while (!done) {
            status = process_entry(dev, flags, new_value, &results[i]);
            done = (status != 0) || (results[i] == COMPLETION_VALUE);
        }

        /* Use log_debug() to help developers understand why error values are
         * being returned */
        if (status != 0) {
            log_debug("Failed to finish operation: %s\n",
                      bladerf_strerror(status));
            goto out;
        }

        /* Braces are always preferred for single-statement conditionals */
        if (results[i] == 0x00 || results[i] == 0xff) {
            results[i] = 0x01;  /* Clamp value because ... such and such ... */
        }

        /* ... Other operations .. */

    }

out:
    if (status != 0) {
        free(results);
        *output_array = NULL;
    } else {
        *output_array = results;
    }

    return status;
}
```
--------------------------------------------------------------------------------

### Use of `goto` ###

As shown in the above example, the use of `goto` is generally restricted to
jumping to a single exit or clean-up point in a function. Using `goto` in this
manner is greatly preferred over complicated nesting and extraneous status
checks.

Other uses of `goto` are not preferred, and will usually indicate that some
changes to the flow of conditionals, state machines, etc., will be requested
before the associated changes will be accepted.


### Types ###

Use types that make your intent explicit to the reader. For example:

 * Use `size_t` for the size of some binary data.
 * Use `unsigned` types for values that should never be negative.
 * Use `bool` for values that should only be true or false.

When operating on variables associated with fixed-size values, such as
device registers, use the appropriate type from `stdint.h`.  Even if you
know with certainty that "unsigned int is 32 bits on platform X,"
use `uint32_t` for the sake of being explicit (and portable).

#### Typedefs ####

Avoid unnecessary typedefs. Stick to POSIX types and native types
unless you have a very good reason.

You may typedef enumerations. Ensure the enumeration values are reasonably
named to avoid conflicts.

**Do not** typedef structures. The `struct` keyword reminds
developers what they are declaring may be a relatively large structure that should
be heap-allocated or passed to functions via a pointer.

Should you use `typedef`...

**Do not** use a ```_t``` suffix. This suffix is [reserved for POSIX types][POSIX].

**Do not** typedef pointers. This is intended to ensure that it is clear what
is and is not a pointer when a reader is looking at declarations and usages.

For example, this is **not** acceptable:
`typedef mystruct* mystruct_p;`

[POSIX]: http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_02_02 "POSIX Name Space"

### Inline functions ###

Prefer the use of static inline functions over macros in order to benefit
from type safety.

Inline functions should be small (a few lines) if they're to be used frequently.

Larger inline functions are fine if they're used once just to break a larger
routine into logical components.

### C89 compliance (Host-code only) ###

In order to appease the MSVC compilers, we have unfortunately have to stick
to some degree of C89 compliance. Specifically, to ensure code compiles with
compilers shipped with Visual Studio:

 * Variable definitions must occur at the beginning of a scope.

 * Do not use named initializers. See the `FIELD_INIT` macro in
   `host_config.h(.in)` to work around this.

   **Note** that this implies that structure members must be initialized in
   order (for MSVC compatiblity).  As such, `FIELD_INIT` serves more of a
   documentation purpose. See the below usage example.
<pre><code>
        struct mystruct test = {
            FIELD_INIT(.field1, 0x01),
            FIELD_INIT(.field2, my_func_ptr),
            FIELD_INIT(.field3, "Some const str"),
        };
</code></pre>

### `const` ###

Use `const` liberally to help annotate your intent to not change the specified
item(s). This is intended to help self-document code and establish clear
interfaces contracts.


### `assert()` ###

Return error values in cases where an invalid condition or input may be
coming from user input. For example, code should not land in `assert()` because
a user provided an out of range value.  An error value, potentially with
a descriptive log message, is more appropriate.

In cases where the above does not apply, use `assert()` to ensure any
preconditions or expected conditions are met.

Also use `assert()` to help detect erroneous states indicative of bugs. Others
may change your code code directly or code that uses your code under the hood.
Use of `assert()` can help bring bugs to light before they are merged upstream.

Use the "rel_assert.h" header file rather than <assert.h>. The former includes
assertions in release code. It is preferred that code grinds to a halt under
specific conditions, causing users to report issues, rather than yield strange
and potentially intermittent behavior.


### Conversions ###

See `host/common/include/conversions.h` for various conversion routines, such
as string to value conversions.

Add useful conversions here so that they may be used throughout the codebase.

### Min/Max Routines ###

See `host/common/include/minmax.h for min/max routines.`

Do not add additional `MIN()` or `MAX()` macros throughout source files. If you
need something that is not present in the above header, add it there.

Favor the routines in the above header over manual conditionals or use of the
ternary operator.

### Debug output ###

Use the macros in `host/common/include/log.h` for writing debug output. This is
required in libbladeRF so that users can control the log verbosity.

When adding debug output, strive to ensure that the output is something
that will be meaningful when included in logs associated with with a bug report.

The desired usage of the log functions are as follows. The functions at the
top of the list should use sparingly. Functions at the bottom of the list
may be used frequently in order to help provide information useful in
diagnosing issues.

#### `log_error()` ####
Used to provide information a situations or conditions that will very likely
lead to a crash or fatal error.

#### `log_warning()` ####
This should be regarded as a less severe form of `log_error()`. Use this to
report conditions which may yield unexpected or undesired behavior.

#### `log_info()` ####
This is the default log level. Use this to provide the user with important
information and reminders that don't warrant the severity of a scary "WARNING"
or "ERROR" message.  If in doubt, you'll generally want to use `log_debug()`
instead of this level.

#### `log_debug()` ####
Use this to report high level status about program flow and to describe the
situations under which `BLADERF_ERR_*` return values are being returned.

#### `log_verbose()` ####
Use this level to provide detailed information about low-level program
operation, such as device register writes.


### Host/OS-specific Items ###

When possible, avoid cluttering the code with `#ifdefs` for host and
OS-specific conditions.

Instead, look to utilize the facilities provided by CMake and this header
template: `host/common/include/host_config.h(.in)`

If you find that the above is insufficient, consider adding to it if your
changes could be used elsewhere in the codebase.

### `ARRAY_SIZE()` ###

`host/common/include/host_config.h(.in)` provides an `ARRAY_SIZE()` macro
intended for determining the number of elements in a constant, contiguous
array. Please use this rather than manually redefining another `(sizeof(x) /
sizeof(x[0]))` macro.

### Threads ###

The libbladeRF and bladeRF-cli code is currently tightly coupled to pthreads.
However, in the future we would like to have the option of migrating to use
native thread support when possible (e.g. in Windows), to reduce dependencies.

To help ease future migration, please use the following macros from
`host/common/include/thread.h`, rather than pthreads items directly:

 * `MUTEX`
 * `MUTEX_INIT(m)`
 * `MUTEX_LOCK(m)`
 * `MUTEX_UNLOCK(m)`

When running the CMake configuration with `-DENABLE_LOCK_CHECKS=Yes`, the above
macros will include status checks and `assert()` failures, which is intended to
help track down deadlock scenarios.

--------------------------------------------------------------------------------






VHDL
================================================================================
Below are the general guidelines for the VHDL portions of the bladeRF project.

### Whitespace ###

 * Spaces only. No tabs.
 * Indents shall be 4 spaces.
 * Vertical whitespace shall **not** indent with spaces to the current
   level of indentation.
 * Use Unix ("\n") line endings (as opposed to DOS "\r\n" line endings)
 * No trailing whitespace at the end of lines


### IEEE Libraries ###

No libraries other than those specified in the IEEE standard shall be used. Some
of the acceptable IEEE-specified libraries include:

 * `std_logic_1164`
 * `numeric_std`
 * `math_real`
 * `math_complex`

More specifically, the `signed` and `unsigned` types will derive from
`numeric_std` and not from `std_logic_arith` or any of the other Synopsys
libraries.

### VHDL-2008 ###

All VHDL shall be compatible with the VHDL-2008 standard and be synthesizable
using the Altera Quartus II synthesis tool as well as the ModelSim simulator.

### Entities ###

All entities that have generics shall write their generics in all capital
letters.

Ports on entities shall have a direction associated with them, preferably `in`
or `out`.  The use of `buffer` can be used in certain circumstances but
sparingly.

### Architectures ###

Multiple architectures for the same entity is allowed and encouraged for similar
functions which may require different resources such as a fully pipelined CORDIC
versus an iterative CORDIC.

If multiple architectures are used with the same entity, it is recommended the
architectures are all within the same VHDL file, but they can be split up.
In these cases, the architecture name shall be representative of the inner workings
of the implementation.

In cases where a single behavioral model is being described, a generic `arch`
or `behavioral` or `rtl` architecture name can be used.

### Records ###

The use of records is encouraged throughout the codebase in places where it is
logical to group signals together.  An example of this may be for a finite state
machine holding state in registers versus combinatorially creating new state.

### Instantiations ###

Creation of components is not recommended.  It may be required in some
circumstances, but it should be avoided if at all possible.

All instantiations are required to have a label preferably prefixed with `U_`
with an appropriate name.  Using the actual architecture name in the
instantiation is recommended as well.

All associations shall be named associations and nothing is positional.  The
associations shall be aligned on a 4-tab character boundary

Instantiation via entity is preferred:
```vhdl
    U_sample_fifo : entity work.fifo(rtl)
      generic map (
        WIDTH       =>  32,
        DEPTH       =>  1024
      ) port map (
        clock       =>  clock,
        reset       =>  reset,

        empty       =>  sample_fifo_empty,
        full        =>  sample_fifo_full,

        din         =>  sample_fifo_din,
        we          =>  sample_fifo_we,

        dout        =>  sample_fifo_dout,
        re          =>  sample_fifo_re
      ) ;
```

### Testbenching ###

Testbenches are highly recommended for every major entity.  It is recommended that
the testbenches be self checking and able to run in an automated fashion.

The typical way for a testbench to finish is with:
<pre><code>
    report "--- End of Simulation ---" severity failure ;
</pre></code>

### IP ###

Major pieces of IP should have a QIP file associated with them for easy inclusion
into a Quartus II project.  Moreover, the library it is compiled into shall be
the name of the piece of IP that is being pulled in or the organization name that
is creating the piece of unique IP.

IP should go in the `hdl/fpga/ip` directory under the organization name that created
or owns the piece of IP.  Any Altera based pieces of IP shall go into the `altera`
directory while all Nuand-based IP shall go in the `nuand` directory.

### Revisions ###

The bladeRF FPGA is managed by using a single generic top-level and a Quartus II
project file with multiple revisions.  Revisions are a feature of the software
which allow different source files as well as SDC files to be brought into the
project and compiled.

When making new functionality for a custom FPGA, it is recommended that a new
revision is created in the `hdl/quartus/bladerf.tcl` file using the
`make_revision` TCL command.

A new QIP file should be added to the `hdl/fpga/platforms/bladerf` directory
with the name of `bladerf-[revision].qip`.
