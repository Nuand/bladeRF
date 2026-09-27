#!/bin/bash

set -e

cat << EOF
To be able to run NiOS's Eclipse the following steps must be taken:
   #1) ./build_bladerf.sh must finish successfully
   #2) Ensure eclipse-nios2 is installed. Read Altera's guide at:
           https://community.altera.com/kb/knowledge-base/is-the-nios%c2%ae-ii-software-build-tools-sbt-for-eclipse-included-in-the-full-instal/340524
   #3) ./launch_eclipse.sh must be provided with a path to the build directory,
         see ./launch_eclipse.sh for example
   #4) Once Eclipse is launched, go to File -> Import -> General -> Existing Projects into Workspace.
   #5) Import bladeRF_nios_bsp by selecting this root directory:
          hdl/fpga/platforms/common/bladerf/software/bladeRF_nios_bsp
   #6) Repeat the import for bladeRF_nios using the target platform's root directory:
          a) bladeRF 1.0: hdl/fpga/platforms/bladerf/software
          b) bladeRF 2.0 micro: hdl/fpga/platforms/bladerf-micro/software
        Confirm the project is named bladeRF_nios, not bladeRF.
   #7) Once imported and available on the Project Explorer pane, right click "bladeRF_nios_bsp" and select "Nios II" -> "Generate BSP"
   #8) Right click "bladeRF_nios" in the Project Explorer pane, and select "Build Project"

Optionally to debug,
   #1) sudo apt install -y libncursesw5   # for Ubuntu 20.04
   #2) Right click "bladeRF_nios" in the Project Explorer pane, and select "Debug As" -> "Debug Configurations..."
   #3) Right click "Nios II Hardware" and select "New", select "New_configuation"
   #4) Under the "Project" tab, fill in the path to the recently generated Nios II ELF file
   #5) Under the "Target connection" tab, check "Ignore mismatched system ID" and "Ignore mismatched system timestamp"
   #6) Find USB-Blaster in "Connections", click "Refresh Connections" if it does not show up
   #7) "Debug" button should be clickable

If on Linux, consider adding a udev rule to grant access to the USB-Blaster to the plugdev group by adding two lines to /etc/udev/rules.d/91-usb.rules:

# Altera
ATTR{idVendor}=="21a9", ATTR{idProduct}=="1004", MODE="660", GROUP="plugdev"

NOTE: The current user should be in 'plugdev' group.

EOF

if [[ -z ${QUARTUS_ROOTDIR+x} ]] ; then
   echo "Could not find Quartus root directory. Is $0 being called from a nios2_command_shell.sh ?"
   exit 1
fi

if ! command -v eclipse-nios2 >/dev/null 2>&1 ; then
   echo "eclipse-nios2 cannot be found. It may have to be manually installed";
   echo "Please try: https://community.altera.com/kb/knowledge-base/is-the-nios%c2%ae-ii-software-build-tools-sbt-for-eclipse-included-in-the-full-instal/340524";
   exit 1
fi


if [[ "$#" -ne 1 ]] ; then
   echo "Usage: $0 <bladeRF HDL build directory>"
   echo "   NOTE: ./build_bladerf.sh must complete successfully before running this script."
   echo ""
   echo "Example: $0 work/bladerf-micro-A4-hosted/"
   echo ""
   if [[ -d work/ ]] ; then
      echo "Found the following potential build directories:"
      for i in $(find work/ -maxdepth 1 -type d | grep -v '^work/$'); do
         echo "   $i";
      done
      echo ""
   fi
   exit 1
fi

if [[ ! -f "$1/settings.bsp" ]] ; then
   echo "Error could not find settings.bsp in '$1'. '$1' may not be a correct build directory.";
   echo "   ./build_bladerf.sh may have to be run again."
   exit 1
fi

if [[ ! -f "$1/nios_system.sopcinfo" ]] ; then
   echo "Error could not find nios_system.sopcinfo in '$1'. '$1' may not be a correct build directory.";
   echo "   ./build_bladerf.sh may have to be run again."
   exit 1
fi

QUARTUS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR=$(cd "$1" && pwd)
export WORKDIR=${BUILD_DIR#"$QUARTUS_DIR/"}

cp "$BUILD_DIR/settings.bsp" "$BUILD_DIR/nios_system.sopcinfo" "$QUARTUS_DIR/../fpga/platforms/common/bladerf/software/bladeRF_nios_bsp/"
if [[ -f "$BUILD_DIR/libad936x/Makefile" ]] ; then
   sed -i 's/# CMAKE generated file: DO NOT EDIT!/unexport LD_LIBRARY_PATH/g' "$BUILD_DIR/libad936x/Makefile"
fi

exec eclipse-nios2
