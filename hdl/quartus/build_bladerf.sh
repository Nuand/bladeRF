#!/usr/bin/env bash
#
# Build a bladeRF fpga image
################################################################################

function usage()
{
    echo ""
    echo "bladeRF FPGA build script"
    echo ""
    echo "Usage: `basename $0` -r <rev> -s <size>" 
    echo ""
    echo "Options:"
    echo "    -r <rev>       FPGA revision"
    echo "    -s <size>      FPGA size"
    echo "    -h             Show this text"
    echo ""
    echo "Supported revisions:"
    echo "    hosted"
    echo "    headless"
    echo "    fsk_bridge"
    echo "    qpsk_tx" 
    echo ""
    echo "Supported sizes (kLE)"
    echo "    40"
    echo "    115"
    echo ""
    echo "This script requires the following items to be in your PATH:"
    echo "    Quartus bin directory"
    echo "    NIOS II EDS bin directory"
    echo "    NIOS II EDS SDK2 directory"
    echo "    NIOS II EDS GCC toolchain bin directory" 
    echo ""
    echo "    Example:"
    echo "    export PATH=\$PATH:/opt/altera/12.1/quartus/bin"
    echo "    export PATH=\$PATH:/opt/altera/12.1/nios2eds/bin"
    echo "    export PATH=\$PATH:/opt/altera/12.1/nios2eds/sdk2/bin"
    echo "    export PATH=\$PATH:/opt/altera/12.1/nios2eds/bin/gnu/H-i686-pc-linux-gnu/bin"
    echo ""
}

if [ $# -eq 0 ]; then
    usage
    exit 0
fi

while getopts ":s:r:h" opt; do
    case $opt in
        h)
            usage
            exit 0
            ;;

        r)
            rev=$OPTARG
            ;;

        s)    
            size=$OPTARG
            ;;

        *)
            echo "Unrecognized option: -$OPTARG" >&2
            usage
            exit 1
            ;;
    esac
done

if [ "$size" == "" ]; then
    echo -e "\nError: size (-s) is required\n" >&2
    usage
    exit 1
fi

if [ "$size" -ne 40 ] && [ "$size" -ne 115 ]; then
    echo -e "\nError: Invalid size (\"$size\")" >&2
    usage
    exit 1
fi

if [ "$rev" == "" ]; then
    echo -e "\nError: revision (-r) is required\n" >&2
    usage
    exit 1
fi

if [ "$rev" != "hosted" ] && [ "$rev" != "headless" ] && \
   [ "$rev" != "fsk_bridge" ] && [ "$rev" != "qpsk_tx" ]; then
    echo -e "\nError: Invalid revision (\"$rev\")\n" >&2
    usage
    exit 1
fi

# quartus_sh script gives a reference point to the quartus bin dir
quartus_sh="`which quartus_sh`"
if [ $? -ne 0 ] || [ ! -f "$quartus_sh" ]; then
    echo -e "\nError: quartus_sh (Quartus 'bin' directory) does not appear to be in your PATH\n" >&2
    exit 1
fi

quartus_sh_path="`dirname $quartus_sh`"
if [ $? -ne 0 ] || [ ! -d "$quartus_sh_path" ]; then
    echo -e "\nError: Could not identify path to quartus_sh\n" >&2
    exit 1
fi

# Ensure the NIOS II EDS dir is in PATH by checking for a known script
niosscript="`which nios2-terminal`"
if [ $? -ne 0 ] || [ ! -f "$niosscript" ]; then
    echo -e "\nError: The NIOS II EDS 'bin' directory does not appear to be in your PATH\n" >&2
    exit 1
fi

# Additionally We appear to need SOPC_KIT_NIOS2 defined for use by some Makefiles
if [ "$SOPC_KIT_NIOS2" = "" ]; then
    export SOPC_KIT_NIOS2="`dirname $niosscript`/../"
    if [ $? -ne 0 ] || [ ! -d "$SOPC_KIT_NIOS2" ]; then
        echo -e "\nError: Failed to set up SOPC_KIT_NIOS2\n" >&2
        exit 1
    fi
fi

# ... and likewise for the NIOS II EDS SDK2 ...
nios2_bsp_generate_files="`which nios2-bsp-generate-files`"
if [ $? -ne 0 ] || [ ! -f "$nios2_bsp_generate_files" ]; then
    echo -e "\nError: The NIOS II EDS SDK2 'bin' directory does not appear to be in your PATH\n" >&2
    exit 1
fi

# ... and the GCC toolchain ...
nios2_gcc="`which nios2-elf-gcc`"
if [ $? -ne 0 ] || [ ! -f "$nios2_gcc" ]; then
    echo -e "\nError: The NIOS II GCC toolchain 'bin' directory does not appear tobe in your PATH\n" >&2
    exit 1
fi

ip_generate=$quartus_sh_path/../sopc_builder/bin/ip-generate
nios_system=../fpga/ip/altera/nios_system

# Error out at the first sign of trouble
set -e

echo ""
echo "##########################################################################"
echo "    Generating NIOS II Qsys for bladeRF ..."
echo "##########################################################################"
echo ""

$ip_generate \
    --project-directory=$nios_system \
    --output-directory=$nios_system \
    --report-file=bsf:$nios_system/nios_system.bsf \
    --system-info=DEVICE_FAMILY=Cyclone\ IV\ E \
    --system-info=DEVICE=EP4CE40F23C8 \
    --system-info=DEVICE_SPEEDGRADE=8 \
    --component-file=$nios_system/nios_system.qsys

$ip_generate \
    --project-directory=$nios_system \
    --output-directory=$nios_system/synthesis \
    --file-set=QUARTUS_SYNTH \
    --report-file=sopcinfo:$nios_system/nios_system.sopcinfo \
    --report-file=html:$nios_system/nios_system.html \
    --report-file=qip:$nios_system/synthesis/nios_system.qip \
    --report-file=cmp:$nios_system/nios_system.cmp \
    --system-info=DEVICE_FAMILY=Cyclone\ IV\ E \
    --system-info=DEVICE=EP4CE40F23C8 \
    --system-info=DEVICE_SPEEDGRADE=8 \
    --component-file=$nios_system/nios_system.qsys

echo ""
echo "##########################################################################"
echo "    Building BSP and sample application..."
echo "##########################################################################"
echo ""

pushd $nios_system/software/lms_spi_controller_bsp
cp ../settings.bsp.in ./settings.bsp
$nios2_bsp_generate_files --settings=settings.bsp --bsp-dir=.
make
cd ../lms_spi_controller
make

# Encountered issues on Ubuntu 13.04 with the SDK's scripts not resolving
# paths properly. In the end, some items wind up being defined as .jar's that
# should be in our PATH at this point, so we set these up here...

#ELF2HEX=elf2hex.jar ELF2DAT=.jar make mem_init_generate
make mem_init_generate
popd


echo ""
echo "##########################################################################"
echo "    Building FPGA Image: $rev, $size kLE"
echo "##########################################################################"
echo ""

mkdir -p work
pushd work
$quartus_sh -t ../bladerf.tcl
$quartus_sh -t ../build.tcl -rev $rev -size $size
popd

RBF="$rev"x"$size".rbf
cp "work/output_files/$rev.rbf" $RBF

echo ""
echo "##########################################################################"
echo "    Done! Image copied to: $rev"x"$size.rbf"
echo "##########################################################################"
echo ""
