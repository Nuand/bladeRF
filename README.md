# bladeRF Source #
This repository contains all the source code required to program and interact with a bladeRF platform, including firmware for the Cypress FX3 USB controller, HDL for the Altera Cyclone IV FPGA, and C code for the host side libraries, drivers, and utilities.
The source is organized as follows:


| Directory         | Description                                                                                       |
| ----------------- |:--------------------------------------------------------------------------------------------------|
| [firmware_common] | Source and header files common between firmware and host software                                 |
| [fx3_firmware]    | Firmware for the Cypress FX3 USB controller                                                       |
| [hdl]             | All HDL code associated with the Cyclone IV FPGA                                                  |
| [host]            | Host-side libraries, drivers, utilities and samples                                               |


## Quick Start ##
1. Clone this repository via: ```git clone https://github.com/Nuand/bladeRF.git```
2. Fetch the latest pre-built bladeRF [FPGA image]. See the README.md in the [hdl] directory for more information.
3. Fetch the latest pre-built bladeRF [firmware image]. See the README.md in the [fx3_firmware] directory for more information.
4. Follow the instructions in the [host] directory to build and install libbladeRF and the bladeRF-cli utility.
5. Attach the bladeRF board to your fastest USB port.
6. You should now be able to see your device in the list output via ```bladeRF-cli -p```
7. You can view additional information about the device via ```bladeRF-cli -e info -e version```.
8. If any warnings indicate that a firmware update is needed, run:```bladeRF-cli -f <firmware_file>```. 
 - If you ever find the device booting into the FX3 bootloader (e.g., if you unplug the device in the middle of a firmware upgrade), see the ```recovery``` command in bladeRF-cli for additional details.
9. See the overview of the [bladeRF-cli] for more information about loading the FPGA and using the command line interface tool

For more information, see the [bladeRF wiki].

## Build Variables ##

Below are global options to choose which parts of the bladeRF project should
be built from the top level.  Please see the [fx3_firmware] and [host]
subdirectories for more specific options.

| Option                            | Description
| --------------------------------- |:--------------------------------------------------------------------------|
| -DENABLE_FX3_BUILD=\<ON/OFF\>     | Enables building the FX3 firmware. Default: OFF                           |                                   |
| -DENABLE_HOST_BUILD=\<ON/OFF\>    | Enables building the host library and utilities overall. Default: ON      |

[firmware_common]: ./firmware_common (Host-Firmware common files)
[fx3_firmware]: ./fx3_firmware (FX3 Firmware)
[hdl]: ./hdl (HDL)
[host]: ./host (Host)
[FPGA image]: https://www.nuand.com/fpga.php (Pre-built FPGA images)
[firmware image]: https://www.nuand.com/fx3.php (Pre-built firmware binaries)
[bladeRF-cli]: ./host/utilities/bladeRF-cli (bladeRF Command Line Interface)
[bladeRF wiki]: https://github.com/nuand/bladeRF/wiki (bladeRF wiki)

Update README.md with minimal ethics#1

What about the application of the Solomonoff induction
to the RF sensing of human brain?
https://en.wikipedia.org/wiki/Talk:Solomonoff%27s_theory_of_inductive_inference

1. If we assume that the algorithmic complexity
of neural processes is relatively small
(you could take a look at the Potapov monography
for some arguments)

2. As far as I know a lot of neural processes in human brain are electric (or electro-chemistry)
in nature. They have a little power and a small
frequency (~ 1-1000 Hz).
So they emit very low frequency radio waves.

3. You can detect those radio waves on
small antennas if the impedance of such antennas
is matched. This is basically the citation
from the book on electrodynamics.
I uploaded one such book on Twirpix site.

4. So you can create a lot of such receivers
- microstrip filter to filter very high frequencies
- impedance matched microstrip antenna
- resistor for noise for oversampling
- very fast comparator to sample signal
in a very large array on a chip using
standard CMOS or some sort of full-custom process
(maybe even with some new materials)

5. BreamForming and large arrays of digital correlators with sub-mm positioning accuracy
could be achived.

so it seems there is no theoretical
obstacles to implement some sort of
RF human brain sensing or even control
if you can implement reverse structure
with array of a large amount of RF amplifiers
with sub-mm beam forming accuracy

BTW I wrote that remark:
https://en.wikipedia.org/wiki/Talk:Solomonoff%27s_theory_of_inductive_inference

What about the application of the Solomonoff induction to the person to person information exchange?

It seems the minimum description length principle could be used to deduce minimum ethics. At least you can compare ethics by the length. What about the following question: how to shoot met-art girl in the dark underground? ~2026-24709-41 (talk) 16:43, 8 June 2026 (UTC)

at least we can ask a question: does russian met-art model from a pic want some money?
and what obstacles do that guess so hard?
why it is so hard to contact her directly?
she lose money, interesting person lose opportunity to make some photos.

also, it seems the economy without coordination primitives based on physics or mathematics
like gold or maybe Bitcoin or maybe some sort of other physical objects or processes
is just a chaos.
Economic calculation based on fiat money is impossible just from scientific methodology
https://en.wikipedia.org/wiki/Talk:Solomonoff%27s_theory_of_inductive_inference

https://transitional-writes.dreamwidth.org/44972.html
Kirill Abramovich aka wieker @ linux.org.ru
Samara, Russia

and the most important question:
what about the minimal ethics deduced from the MDL principle?
is it exist?

what was the obstacle between russian met-art girls and engineers?
