# AD9361 no-OS v1.251.0 Migration

Date: 2026-09-02

## Position

This branch starts at bladeRF master `25151f2c1abb62da41c4b4c1792e0eb48666cca5`
and updates the Analog Devices no-OS submodule from
`0bba46e6f6f75785a65d425ece37d0a04daf6157` to the v1.251.0 commit
`e31142c6e7b08e6b4cee40f1997aef3b48cbca79`.

The completed ADRV prototype at `156a46b4da9d3bed97909cad8a594dde7c3391ad`
was reviewed as migration evidence. No ADRV or Navassa host, HDL, profile,
diagnostic, channel-map, or lifecycle implementation was carried into this
branch.

## Problem

The old integration depended on removed platform ADC and DAC cores and the
pre-v1.251 no-OS platform API. The prototype migration also left the host AXI
implementation on a three-argument interface after adding a context argument
to `no_os_axi_io.h`. Its global AXI-device workaround could not safely identify
the owning bladeRF when more than one device was open.

The migration also had to preserve bladeRF2's established AD9361 initialization
values and failure behavior. A mechanical copy of the ADRV branch would have
mixed that work with unrelated product changes and temporary coexistence
hacks.

## Possibilities reviewed

* Cherry-picking the prototype history was rejected because it would import
  ADRV-specific implementation and an obsolete merge base.
* Keeping the legacy ADC and DAC platform cores was rejected because v1.251.0
  provides the maintained AXI cores and ownership model.
* Keeping a global AXI device was rejected. AXI reads and writes now use the
  context passed through AD9361 and the AXI descriptors.
* The prototype's temporary AD9361-disable patch was rejected because this is
  an AD9361-only production integration.
* The prototype's xparameters suppression and ADRV timing patch were rejected;
  neither is required by the AD9361 migration.
* Later commits `36a41f0f1` and `62486a099` were reviewed line by line. Their
  vendor-header split, Navassa build-definition removal, and conditional shared
  gain tables solve concurrent AD9361/Navassa compilation. None is required by
  the AD9361-only dependency graph, so that independent build refactor and its
  Navassa call-site changes were not ported.

## Proposal implemented

Migration commit `2114eac4c` and its focused fixups port the dependency on top
of current master, use the upstream v1.251.0 AXI cores, supply separate host
and Nios platform operations, pass the owning bladeRF as AXI context, and
retain AD9361 initialization behavior where the new API still supports it.
The profiles keep legacy independent-FDD mode enabled. They disable the new
managed TX LO power-down behavior because the old driver did not manage TX LO
power this way.

Commit `1792268b1` restores the production timing policy after a candidate FPGA
image exposed intermittent three-buffer RX reordering. Production builds skip
digital-interface tuning as before, timing-verification builds request it, and
the fast-AGC initialization retains the bladeRF2 LVDS bias, termination, phase,
and inversion values. A baseline host with the candidate image reproduced the
reordering, while the exact baseline image did not; the corrected image then
passed 50 consecutive discontinuity runs.

The host sample-rate transition disables the FIR at a safe 30 MHz before
crossing the FIR-by-four range. This permanently covers the reproduced
42,492,989 Hz failure. The gain ranges now match the tables accepted by the
v1.251.0 driver.

Focused follow-on changes preserve the configured X2 RX backend, propagate
stream failures through teardown, and decode the v1.251 signed FPGA RX gain.
The Nios RFIC transport now keeps one receive pending for slow no-OS commands,
validates the response target, direction, and address, and drains one stale
response without sending a duplicate command. The bladeRF2 loopback mode query
also omits host-only RFIC BIST while FPGA tuning is active.

## Local patch audit

The retained `0009` through `0015` names correspond to the individually
reviewed prototype patches. Patches numbered `0016` through `0021` below are
new, AD9361-only local patches; they are not the prototype patches that used
those numbers.

* `0009`: retained to use the v1.251 AXI ADC write API for the bladeRF rate
  register.
* `0010`: retained to preserve the bladeRF DAC rate-control low bit.
* `0011`: retained to carry descriptor userdata into every AXI ADC and DAC
  access.
* `0012`: retained to add context to the generic AXI I/O contract.
* `0013`: retained to copy the RX AXI initializer and attach caller context.
* `0014`: retained to accept context at the AD9361 initialization boundary.
* `0015`: retained to store userdata in allocated AXI ADC and DAC owners.
* `0016`: retained to correct v1.251 diagnostics and initialization warnings
  under `-Werror`.
* `0017`: retained to restore bladeRF2 delay, inversion, initialization, and
  invalid-eye behavior.
* `0018`: retained to release the AXI ADC owner during AD9361 teardown.
* `0019`: retained for Nios only to remove the unused FIR text parser because
  the supported small C library has no `sscanf`.
* `0020`: retained for Nios only to suppress unconditional success text that
  fills the command UART and stalls Nios. Host diagnostics remain enabled.
* `0021`: retained for host compiler portability. It uses the standard Boolean
  type on MSVC, avoids a Windows macro collision, and makes diagnostics valid
  ISO C statements.

The former `0001` through `0006` patch set was removed only after its behavior
was found in v1.251.0 or represented by the focused replacements above. The
prototype's temporary `0016-disable-ad9361.patch` was rejected because it
bypasses AD9361 setup. Its `0017-remove-xparameters-h.patch` was rejected after
the A4, A5, and A9 Nios graphs built without that suppression. Its
`0018-dac-rate-init.patch` was rejected independently because the extra ADRV
startup write is absent from the AD9361 initialization contract and would
overwrite the bladeRF-specific rate-control value retained by `0010`.

## Validation

### Builds

* Final clean Debug and Release host configurations used `BUILD_NAVASSA=OFF`,
  `BUILD_AD936X=ON`, `TEST_LIBBLADERF=ON`, and
  `TREAT_WARNINGS_AS_ERRORS=ON`; both completed with GCC 15.2.
* The AD9361 Nios firmware compiled for bladeRF-micro A4, A5, and A9. Full A4
  and A9 FPGA builds passed TimeQuest. Minimum setup/hold slack was
  +0.797/+0.059 ns on A4 and +0.522/+0.010 ns on A9.
* Final volatile image SHA-256 values were
  `92d89318020995eb5b99e08b3b0771eaa77f4193411be1c6c1cf378d16287dc0`
  for A4 and
  `6dcb70db190ba690fe16ec5c0b2729933636c89ac65aed7a5518fe1ae75cb82e`
  for A9.
* A2 remains unsupported because its generated address map overlaps; it is not
  a supported bladeRF2 FPGA target for this integration.
* CTest exits successfully but reports `No tests were found!!!`; this result is
  not regression coverage.

### Continuous integration

The software and hardware workflows used source `3e3f2879f` with CI overlay
`0c474ee7b`. Ubuntu (`33715086702`), HIL (`33715086716`), FreeBSD
(`33715086742`), macOS (`33715086744`), Windows (`33715086801`), and the
13-image Docker compiler matrix (`33715086805`) all completed successfully.

FPGA diagnostic run `33713362010` failed only the bladeRF-micro foxhunt A4 and
A9 cases, where the migrated AD9361 library could not resolve the shared no-OS
platform functions. Adding `no_os_platform.c` to the foxhunt firmware sources
made focused A4 and A9 builds pass. Exact rerun `33717170223` used source
`c80bdbdf8` with CI overlay `0c474ee7b`; all 11 `bitfile_ci` targets passed and
the workflow produced its FPGA report and 11 bitfile artifacts.

### Software checks

Valgrind ran with full leak checking and an error exit code on the final Debug
build's C and C++ API smoke tests, `version`, `interleaver`, `parse`,
`flash_fpga_compression`, `sync_worker_stop`, `libusb_event_wait`, and
`gain_range_lock` tests. All nine returned zero with all heap blocks freed and
no errors. These are software-only tests and do not open an unspecified
bladeRF.

### AD9361 hardware

The permitted fixtures were selected by exact serial:

* 49-KLE xA4 on Acroname port 4:
  `e7841bcdfc4349c98c1264fdf8d7d7f9`
* 301-KLE xA9 on Acroname port 0:
  `52f4b4c4e1164ce3a7b89d3e47c8c0e8`

The final matrix used the xA4 on port 4 and excluded port 3 and its device.
FPGA images were loaded volatilely with lowercase `-l`.

On both boards and in both host and FPGA tuning modes, `sampling`, `lpf_mode`,
`enable_module`, `loopback`, `rx_mux`, `correction`, `samplerate`, `bandwidth`,
`gain`, `frequency`, and `threads` returned zero: 44 green cases. FPGA tuning
advertises and exercises `none` and firmware loopback; host tuning additionally
exercises RFIC BIST.

Both boards and tuning modes passed 1,000-iteration RX-counter discontinuity
checks and ten repeated RX/TX stream start-stop cycles at 15.36 MSPS. They also
passed 25 repeated open/close cycles without a reset. X1 synchronous and native
asynchronous RX/TX passed at 1, 15.36, and 61.44 MSPS; X2 passed at 1 and
15.36 MSPS. The final 80-case matrix covered both boards, both tuning modes,
both directions, and both synchronous and asynchronous APIs. Every native
asynchronous case completed eight callbacks; both X2 RX lanes contained
nonzero samples.

One first-pass xA9 host-tuned X1 RX case at 61.44 MSPS timed out while stopping
a worker. The exact case then passed 20 consecutive candidate runs and
reproduced on the original master host and image on run 29. A complete no-retry
rerun passed all 80 cases, so the intermittent teardown timeout remains an
original regression rather than a migration regression.

Recovery coverage used the discontinuity test, ten repeated RX/TX starts and
stops, 25 opens and closes, a deterministic missing-image error, and a clean
15.36 MSPS RX after that error. All five cases passed for both boards and both
tuning modes. The pre-fix candidate image reordered three adjacent RX buffers
in one of 30 cross-version runs; the corrected image passed 50 of 50 focused
runs and the final four-way recovery matrix with zero gaps.

### Qualification boundary

The dependency-only branch builds the AD9361 host and supported Nios paths and
passes the original AD9361 hardware matrix. The completed ADRV branch was then
used only to confirm the shared dependency: its no-OS submodule is the same
`e31142c6e7b08e6b4cee40f1997aef3b48cbca79` commit. No ADRV implementation is
present in this branch.

On the exact zero-serial ADRV mapped passively to Acroname hub `5263150C` port
6, the established 15.36 MSPS acceptance passed 7/7 independently cold-booted
gates: boot, SSI, RX DMA, TX DMA, both X1 RF paths, and distinct-tone X2. The
recorded timing-clean image was loaded volatilely with lowercase `-l`. X1
prominence was 44.7 and 41.2 dB. X2 found bins 256 and 896 with 47.8 and
40.1 dB prominence and 22.3 and 26.8 dB expected-tone isolation. Every hub
state change in this acceptance targeted port 6 only; port 3 and its bladeRF
were excluded.

These results qualify the retained no-OS dependency for the established AD9361
and ADRV functional paths. They do not import or qualify ADRV/Navassa product
implementation on this dependency-only branch, nor do they expand the ADRV
claim beyond its established 15.36 MSPS functional acceptance.
