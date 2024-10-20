#!/bin/bash

cat << EOF
To be able to run NiOS's Eclipse the following steps must be taken:
   #1) ./build_bladerf.sh must finish successfully
   #2) Ensure eclipse-nios2 is installed. Read Intel's guide at:
           https://www.intel.com/content/www/us/en/support/programmable/articles/000086893.html
   #3) Replace Quartus bundled JVM with OpenJDK 8:
         Reference: https://community.intel.com/t5/Intel-Quartus-Prime-Software/Nios-Eclipse-crashes-free-invalid-pointer/m-p/1215921#M66524
         sudo apt update
         sudo apt install openjdk-8-jdk
         cd <path to quartus>/intelFPGA/20.1/quartus/linux64
         mv jre64 jre64_old
         ln -s /lib/jvm/java-1.8.0-openjdk-amd64/jre jre64
   #4) ./launch_eclipse.sh must be called from hdl/quartus and provided with a
         path to the build directory, see ./launch_eclipse.sh for example
   #5) Once Eclipse is launched, go to File -> Import -> General -> Existing Projects into Workspace.
   #6) Click "Select root directory" and supply the path to the bladeRF git repository.
   #7) Select and import 2 projects: the bladeRF_nios_bsp project and the corresponding bladeRF project for the target bladeRF:
          a) bladeRF_nios_bsp should be in hdl/fpga/platforms/common/bladerf/software/bladeRF_nios_bsp/
          b) bladeRF (select hdl/fpga/platforms/bladerf for bladeRF 1.0 targets, select hdl/fpga/platforms/bladerf-micro for bladeRF 2.0 targets)
   #8) Once imported and available on the Project Explorer pane, right click "bladeRF_nios_bsp" and select "Nios II" -> "Generate BSP"
   #9) Right click "bladeRF" in the Project Explorer pane, and select "Build Project"

Optionally to debug,
   #1) sudo apt install -y libncursesw5   # for Ubuntu 20.04
   #2) Right click "bladeRF" in the Project Explorer pane, and select "Debug As" -> "Debug Configurations..."
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

if [[ ! `which eclipse-nios2` ]] ; then
   echo "elcipse-nios2 cannot be found. It may have to be manually installed";
   echo "Please try: https://www.intel.com/content/altera-www/global/en_us/index/support/support-resources/knowledge-base/tools/2019/why-does-the-nios--ii-not-installed-after-full-installation-of-t.html";
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
      for i in `find work/ -maxdepth 1 -type d | grep -v '^work/$'`; do
         echo "   $i";
      done
      echo ""
   fi
   exit 1
fi

if [[ ! -f $1/settings.bsp ]] ; then
   echo "Error could not find settings.bsp in '$1'. '$1' may not be a correct build directory.";
   echo "   ./build_bladerf.sh may have to be run again."
   exit 1
fi

if [[ ! -f $1/nios_system.sopcinfo ]] ; then
   echo "Error could not find nios_system.sopcinfo in '$1'. '$1' may not be a correct build directory.";
   echo "   ./build_bladerf.sh may have to be run again."
   exit 1
fi

export WORKDIR=$1

cp $1/settings.bsp $1/nios_system.sopcinfo ../fpga/platforms/common/bladerf/software/bladeRF_nios_bsp/
sed -i 's/# CMAKE generated file: DO NOT EDIT!/unexport LD_LIBRARY_PATH/g' $1/libad936x/Makefile

eclipse-nios2
