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
    echo "    -c                    Clear working directory"
    echo "    -b <board>            Target board"
    echo "    -r <rev>              FPGA revision"
    echo "    -s <size>             FPGA size"
    echo "    -a <stp>              SignalTap STP file"
    echo "    -f                    Force STP insertion"
    echo "    -n <Tiny|Small|Fast>  Select NIOS II revision. Tiny is the"
    echo "                            default and can be built with"
    echo "                            Quartus Web Edition."
    echo "    -h                    Show this text"
    echo ""

    echo "Supported boards:"
    for i in ../fpga/platforms/*/build/platform.conf ; do
        source $i
        echo "    [*] $BOARD_NAME";
        echo "        Supported revisions:"
        for rev in ${PLATFORM_REVISIONS[@]} ; do
            echo "            ${rev}"
        done
        echo "        Supported sizes (kLE):"
        for size in ${PLATFORM_FPGA_SIZES[@]} ; do
            echo "            ${size}"
        done
        echo ""
    done
    echo ""
}

pushd () {
    command pushd "$@" >/dev/null
}

popd () {
    command popd "$@" >/dev/null
}

# Current Quartus version
declare -A QUARTUS_VER # associative array
QUARTUS_VER[major]=0
QUARTUS_VER[minor]=0

# Parameters:
#   $1 Expected major Quartus version
#   $2 Expected minor Quartus version
# Returns:
#   0 on compatible version
#   1 on incompatible version or unable to detemine version
check_quartus_version()
{
    local readonly exp_major_ver="$1"
    local readonly exp_minor_ver="$2"
    local readonly exp_ver="${exp_major_ver}.${exp_minor_ver}"

    local readonly VERSION_FILE="${QUARTUS_ROOTDIR}/version.txt"

    if [ ! -f "${VERSION_FILE}" ]; then
        echo "Could not find Quartus version file." >&2
        return 1
    fi

    local readonly VERSION=$( \
        grep -m 1 Version "${QUARTUS_ROOTDIR}/version.txt" | \
        sed -e 's/Version=//' \
    )

    echo "Detected Quartus II ${VERSION}"

    QUARTUS_VER[major]=$( \
        echo "${VERSION}" | \
        sed -e 's/\([[:digit:]]\+\).*/\1/g' \
    )

    QUARTUS_VER[minor]=$( \
        echo "${VERSION}"   | \
        sed -e 's/^16\.//g' | \
        sed -e 's/\([[:digit:]]\+\).*/\1/g' \
    )

    if [ -z "${QUARTUS_VER[major]}" ] ||
           [ -z "${QUARTUS_VER[minor]}" ]; then
        echo "Failed to retrieve Quartus version number." >&2
        return 1
    fi

    if [ $(expr ${QUARTUS_VER[major]}\.${QUARTUS_VER[minor]} \< ${exp_ver}) -eq 1 ]; then
        echo "The bladeRF FPGA design requires Quartus II version ${exp_ver}" >&2
        echo "The installed version is: $VERSION" >&2
        return 1
    fi

    return 0
}

if [ $# -eq 0 ]; then
    usage
    exit 0
fi

# Default to NIOS II E (Tiny)
nios_rev="Tiny"

while getopts ":fa:b:s:r:n:h:c" opt; do
    case $opt in
        c)
            clear_work_dir=1
            ;;

        h)
            usage
            exit 0
            ;;

        b)
            board=$OPTARG
            ;;

        r)
            rev=$OPTARG
            ;;

        s)
            size=$OPTARG
            ;;

        a)
            echo "STP: $OPTARG"
            stp=$(readlink -f $OPTARG)
            ;;

        f)
            echo "Forcing STP insertion"
            force="-force"
            ;;

        n)  nios_rev=$OPTARG
            ;;

        \?)
            echo "Unrecognized option: -$OPTARG" >&2
            usage
            exit 1
            ;;
    esac
done

if [ "$clear_work_dir" == "1" ]; then
    echo -e "\nClearing work/ directory\n" >&2
    rm -rf work
fi

if [ "$board" == "" ]; then
    echo -e "\nError: board (-b) is required\n" >&2
    usage
    exit 1
fi

if [ "$size" == "" ]; then
    echo -e "\nError: size (-s) is required\n" >&2
    usage
    exit 1
fi

if [ "$rev" == "" ]; then
    echo -e "\nError: revision (-r) is required\n" >&2
    usage
    exit 1
fi

for plat in ../fpga/platforms/*/build/platform.conf ; do
    source $plat
    if [ $board == "$BOARD_NAME" ]; then
        platform=$(basename $(dirname $(dirname $plat)))
        break
    fi
done

if [ "$platform" == "" ]; then
    echo -e "\nError: Invalid board (\"$board\")\n" >&2
    exit 1
fi

for plat_size in ${PLATFORM_FPGA_SIZES[@]} ; do
    if [ "$size" == "$plat_size" ]; then
        size_valid="yes"
        break
    fi
done

if [ "$size_valid" == "" ]; then
    echo -e "\nError: Invalid size (\"$size\")" >&2
    usage
    exit 1
fi

for plat_rev in ${PLATFORM_REVISIONS[@]} ; do
    if [ "$rev" == "$plat_rev" ]; then
        rev_valid="yes"
        break
    fi
done

if [ "$rev_valid" == "" ]; then
    echo -e "\nError: Invalid revision (\"$rev\")\n" >&2
    usage
    exit 1
fi

if [ "$stp" != "" ] && [ ! -f "$stp" ]; then
    echo -e "\nCould not find STP file: $stp\n" >&2
    exit 1
fi

nios_rev=$(echo "$nios_rev" | tr "[:upper:]" "[:lower:]")
if [ "$nios_rev" != "tiny" ] && [ "$nios_rev" != "small" ] && [ "$nios_rev" != "fast" ]; then
    echo -e "\nInvalid NIOS II revision: $nios_rev\n" >&2
    exit 1
fi

DEVICE=$(get_device $size)

# quartus_sh script gives a reference point to the quartus bin dir
quartus_sh="`which quartus_sh`"
if [ $? -ne 0 ] || [ ! -f "$quartus_sh" ]; then
    echo -e "\nError: quartus_sh (Quartus 'bin' directory) does not appear to be in your PATH\n" >&2
    exit 1
fi


# Complain early about an unsupported version. Otherwise, the user
# may get some unintuitive error messages.
check_quartus_version ${PLATFORM_QUARTUS_VER[major]} ${PLATFORM_QUARTUS_VER[minor]}
if [ $? -ne 0 ]; then
    exit 1
fi

# Check compatibility of Nios selection against Quartus version
if [ "$nios_rev" == "small" ] &&
         [ $(expr ${QUARTUS_VER[major]}\.${QUARTUS_VER[minor]} \>\= 14.1) -eq 1 ]; then
    echo ""
    echo "NiosII/s (Small) is deprecated as of Quartus 14.1 and later." >&2
    echo "However, it may be approximated with certain configuration options" >&2
    echo "of the NiosII/f." >&2
    exit 1
fi

nios_system=../fpga/ip/altera/nios_system

# 9a484b436: Windows-specific workaround for Quartus bug
if [ "x$(uname)" != "xLinux" ]; then
    QUARTUS_BINDIR=$QUARTUS_ROOTDIR/bin
    export QUARTUS_BINDIR
    echo "## Non-Linux OS Detected (Windows?)"
    echo "## Forcing QUARTUS_BINDIR to ${QUARTUS_BINDIR}"
fi

# Error out at the first sign of trouble
set -e

work_dir=work/${platform}
common_dir=../../../fpga/platforms/common/bladerf/
build_dir=../../../fpga/platforms/${platform}/build/

mkdir -p ${work_dir}
pushd ${work_dir}

cp -r ${build_dir}/ip.ipx .

if [ -f ${build_dir}/../*.srf ]; then
    cp -r ${build_dir}/../*.srf .
fi

echo ""
echo "##########################################################################"
echo "    Generating NIOS II Qsys for ${board} ..."
echo "##########################################################################"
echo ""

if [ -f common_system.qsys ]; then
    echo "Skipping building common system Qsys";
else
    echo "Building common system Qsys";
    qsys-script \
        --script=${common_dir}/build/common_system.tcl \
        --cmd="set nios_impl ${nios_rev}"
fi

if [ -f nios_system.qsys ]; then
    echo "Skipping building platform Qsys";
else
    echo "Building platform Qsys";
    qsys-script \
        --script=${build_dir}/nios_system.tcl \
        --cmd="set nios_impl ${nios_rev}"
fi

if [ -f ${build_dir}/platform.sh ]; then
    source ${build_dir}/platform.sh
fi

if [ -f nios_system.sopcinfo ]; then
    echo "Skipiping qsys-generate, nios_system.sopcinfo already exists"
else
    qsys-generate --synthesis=Verilog nios_system.qsys
fi

echo ""
echo "##########################################################################"
echo "    Building BSP and ${board} application..."
echo "##########################################################################"
echo ""

mkdir -p bladeRF_nios_bsp
if [ -f settings.bsp ]; then
    echo "Skipping creating Nios BSP, settings.bsp already exists"
else
    nios2-bsp-create-settings \
        --settings settings.bsp \
        --type hal \
        --bsp-dir bladeRF_nios_bsp \
        --cpu-name common_system_0_nios2 \
        --script $(readlink -f $QUARTUS_ROOTDIR/..)/nios2eds/sdk2/bin/bsp-set-defaults.tcl \
        --sopc nios_system.sopcinfo \
        --set hal.max_file_descriptors 4 \
        --set hal.enable_instruction_related_exceptions_api false \
        --set hal.make.bsp_cflags_optimization "-Os" \
        --set hal.enable_exit 0 \
        --set hal.enable_small_c_library 1 \
        --set hal.enable_clean_exit 0 \
        --set hal.enable_c_plus_plus 0 \
        --set hal.enable_reduced_device_drivers 1 \
        --set hal.enable_lightweight_device_driver_api 1
fi

pushd bladeRF_nios_bsp

sed -i.bak 's/\($(ELF2HEX).*--width=$(mem_hex_width)\)/\1 --record=$(shell expr ${mem_hex_width} \/ 8)/g' mem_init.mk

make
popd

pushd ${build_dir}/../software/bladeRF_nios/

# Encountered issues on Ubuntu 13.04 with the SDK's scripts not resolving
# paths properly. In the end, some items wind up being defined as .jar's that
# should be in our PATH at this point, so we set these up here...

#ELF2HEX=elf2hex.jar ELF2DAT=.jar make mem_init_generate
make mem_init_clean mem_init_generate
popd

echo ""
echo "##########################################################################"
echo "    Building ${board} FPGA Image: $rev, $size kLE"
echo "##########################################################################"
echo ""

$quartus_sh -t ${build_dir}/bladerf.tcl -projname bladerf \
            -part ${DEVICE} -platdir $(readlink -f ${build_dir}/..)

if [ "$stp" == "" ]; then
    $quartus_sh -t ../../build.tcl -projname bladerf -rev hosted -flow full
else
    $quartus_sh -t ../../build.tcl -projname bladerf -rev hosted -flow full -stp $stp $force
fi

popd
BUILD_TIME_DONE="$(cat work/${platform}/output_files/$rev.done)"
BUILD_TIME_DONE=$(date -d"$BUILD_TIME_DONE" '+%F_%H.%M.%S')

BUILD_NAME="$rev"x"$size"
BUILD_OUTPUT_DIR="$BUILD_NAME"-"$BUILD_TIME_DONE"
RBF=$BUILD_NAME.rbf

mkdir -p "$BUILD_OUTPUT_DIR"
cp "work/${platform}/output_files/$rev.rbf" "$BUILD_OUTPUT_DIR/$RBF"

for file in work/${platform}/output_files/*rpt work/${platform}/output_files/*summary; do
    new_file=$(basename $file | sed -e s/$rev/"$BUILD_NAME"/)
    cp $file "$BUILD_OUTPUT_DIR/$new_file"
done

pushd $BUILD_OUTPUT_DIR
md5sum $RBF > $RBF.md5sum
sha256sum $RBF > $RBF.sha256sum
MD5SUM=$(cat $RBF.md5sum | awk '{ print $1 }')
SHA256SUM=$(cat $RBF.sha256sum  | awk '{ print $1 }')
popd

echo ""
echo "##########################################################################"
echo " Done! Image, reports, and checksums copied to:"
echo "   $BUILD_OUTPUT_DIR"
echo ""
echo " $RBF checksums:"
echo "  MD5:    $MD5SUM"
echo "  SHA256: $SHA256SUM"
echo ""
cat "$BUILD_OUTPUT_DIR/$BUILD_NAME.fit.summary" | sed -e 's/^\(.\)/ \1/g'
echo "##########################################################################"
echo ""
