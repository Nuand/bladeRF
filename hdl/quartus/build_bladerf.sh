#!/usr/bin/env bash
#
# Build a bladeRF fpga image
################################################################################

function print_boards() {
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
}

function usage()
{
    echo ""
    echo "bladeRF FPGA build script"
    echo ""
    echo "Usage: `basename $0` -b <board_name> -r <rev> -s <size>"
    echo ""
    echo "Options:"
    echo "    -c                    Clear working directory"
    echo "    -b <board_name>       Target board name"
    echo "    -r <rev>              Quartus project revision"
    echo "    -s <size>             FPGA size"
    echo "    -a <stp>              SignalTap STP file"
    echo "    -f                    Force SignalTap STP insertion by temporarily enabling"
    echo "                          the TalkBack feature of Quartus (required for Web Edition)."
    echo "                          The previous setting will be restored afterward."
    echo "    -n <Tiny|Fast>        Select Nios II Gen 2 core implementation."
    echo "       Tiny (default)       Nios II/e; Compatible with Quartus Web Edition"
    echo "       Fast                 Nios II/f; Requires Quartus Standard or Pro Edition"
    echo "    -l <gen|synth|full>   Quartus build steps to complete. Valid options:"
    echo "       gen                  Only generate the project files"
    echo "       synth                Synthesize the design"
    echo "       full (default)       Fit the design and create programming files"
    echo "    -S <seed>             Fitter seed setting (default: 1)"
    echo "    -h                    Show this text"
    echo ""

    print_boards

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

# Set default options
nios_rev="Tiny"
flow="full"
seed="1"

while getopts ":cb:r:s:a:fn:l:S:h" opt; do
    case $opt in
        c)
            clear_work_dir=1
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

        n)
            nios_rev=$OPTARG
            ;;

        l)
            flow=$OPTARG
            ;;

        S)
            seed=$OPTARG
            ;;

        h)
            usage
            exit 0
            ;;

        \?)
            echo -e "\nUnrecognized option: -$OPTARG\n" >&2
            exit 1
            ;;

        :)
            echo -e "\nArgument required for argument -${OPTARG}.\n" >&2
            exit 1
            ;;
    esac
done

if [ "$board" == "" ]; then
    echo -e "\nError: board (-b) is required\n" >&2
    print_boards
    exit 1
fi

if [ "$size" == "" ]; then
    echo -e "\nError: FPGA size (-s) is required\n" >&2
    print_boards
    exit 1
fi

if [ "$rev" == "" ]; then
    echo -e "\nError: Quartus project revision (-r) is required\n" >&2
    print_boards
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
    echo -e "\nError: Invalid FPGA size (\"$size\")\n" >&2
    print_boards
    exit 1
fi

for plat_rev in ${PLATFORM_REVISIONS[@]} ; do
    if [ "$rev" == "$plat_rev" ]; then
        rev_valid="yes"
        break
    fi
done

if [ "$rev_valid" == "" ]; then
    echo -e "\nError: Invalid Quartus project revision (\"$rev\")\n" >&2
    print_boards
    exit 1
fi

if [ "$stp" != "" ] && [ ! -f "$stp" ]; then
    echo -e "\nCould not find STP file: $stp\n" >&2
    exit 1
fi

nios_rev=$(echo "$nios_rev" | tr "[:upper:]" "[:lower:]")
if [ "$nios_rev" != "tiny" ] && [ "$nios_rev" != "fast" ]; then
    echo -e "\nInvalid Nios II revision: $nios_rev\n" >&2
    exit 1
fi

if [[ ${flow} != "gen" ]] &&
       [[ ${flow} != "synth" ]] &&
       [[ ${flow} != "full" ]]; then
    echo -e "\nERROR: Invalid flow option: ${flow}.\n" >&2
    exit 1
fi

DEVICE_FAMILY=$(get_device_family $size)
DEVICE=$(get_device $size)

# Check for quartus_sh
quartus_check="`which quartus_sh`"
if [ $? -ne 0 ] || [ ! -f "$quartus_check" ]; then
    echo -e "\nError: quartus_sh (Quartus 'bin' directory) does not appear to be in your PATH\n" >&2
    exit 1
fi

# Check for Qsys
qsys_check="`which qsys-generate`"
if [ $? -ne 0 ] || [ ! -f "$qsys_check" ]; then
    echo -e "\nError: Qsys (SOPC builder 'bin' directory) does not appear to be in your PATH.\n" >&2
    exit 1
fi

# Check for Nios II SDK
nios2_check="`which nios2-bsp-create-settings`"
if [ $? -ne 0 ] || [ ! -f "$nios2_check" ]; then
    echo -e "\nError: Nios II SDK (nios2eds 'bin' directory) does not appear to be in your PATH.\n" >&2
    exit 1
fi

# Complain early about an unsupported version. Otherwise, the user
# may get some unintuitive error messages.
check_quartus_version ${PLATFORM_QUARTUS_VER[major]} ${PLATFORM_QUARTUS_VER[minor]}
if [ $? -ne 0 ]; then
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

work_dir="work/${platform}-${size}-${rev}"

if [ "$clear_work_dir" == "1" ]; then
    echo -e "\nClearing ${work_dir} directory\n" >&2
    rm -rf "${work_dir}"
fi

mkdir -p ${work_dir}
pushd ${work_dir}

# These paths are relative to $work_dir
common_dir=../../../fpga/platforms/common/bladerf
build_dir=../../../fpga/platforms/${platform}/build

cp -au ${build_dir}/ip.ipx .

if [ -f ${build_dir}/suppressed_messages.srf ]; then
    cp -au ${build_dir}/suppressed_messages.srf ./${rev}.srf
fi

echo ""
echo "##########################################################################"
echo "    Generating Nios II Qsys for ${board} ..."
echo "##########################################################################"
echo ""

if [ -f nios_system.qsys ]; then
    echo "Skipping building platform Qsys"
else
    echo "Building platform Qsys"
    cmd="set nios_impl ${nios_rev}"
    cmd="${cmd}; set device_family {${DEVICE_FAMILY}}"
    cmd="${cmd}; set device ${DEVICE}"
    cmd="${cmd}; set nios_impl ${nios_rev}"
    cmd="${cmd}; set ram_size $(get_qsys_ram $size)"
    qsys-script \
        --script=${build_dir}/nios_system.tcl \
        --cmd="${cmd}"
fi

if [ -f ${build_dir}/platform.sh ]; then
    source ${build_dir}/platform.sh
fi

if [ -f nios_system.sopcinfo ]; then
    echo "Skipping qsys-generate, nios_system.sopcinfo already exists"
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
        --cpu-name nios2 \
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

# Fix warnings about a memory width mismatch between the Nios and the memory initialization file
sed -i.bak 's/\($(ELF2HEX).*--width=$(mem_hex_width)\)/\1 --record=$(shell expr ${mem_hex_width} \/ 8)/g' mem_init.mk

make
popd

pushd ${build_dir}/../software/bladeRF_nios/

# Encountered issues on Ubuntu 13.04 with the SDK's scripts not resolving
# paths properly. In the end, some items wind up being defined as .jar's that
# should be in our PATH at this point, so we set these up here...

#ELF2HEX=elf2hex.jar ELF2DAT=.jar make mem_init_generate
make WORKDIR=${work_dir} \
     mem_init_clean \
     mem_init_generate

popd

echo ""
echo "##########################################################################"
echo "    Building ${board} FPGA Image: $rev, $size kLE"
echo "##########################################################################"
echo ""

# Generate Quartus project
quartus_sh --64bit \
           -t        "${build_dir}/bladerf.tcl" \
           -projname "${PROJECT_NAME}" \
           -part     "${DEVICE}" \
           -platdir  "${build_dir}/.."

# Run Quartus flow
quartus_sh --64bit \
           -t        "../../build.tcl" \
           -projname "${PROJECT_NAME}" \
           -rev      "${rev}" \
           -flow     "${flow}" \
           -stp      "${stp}" \
           -force    "${force}" \
           -seed     "${seed}"

popd

if [[ ${flow} == "full" ]]; then
    BUILD_TIME_DONE="$(cat ${work_dir}/output_files/$rev.done)"
    BUILD_TIME_DONE=$(date -d"$BUILD_TIME_DONE" '+%F_%H.%M.%S')

    BUILD_NAME="$rev"x"$size"
    BUILD_OUTPUT_DIR="$BUILD_NAME"-"$BUILD_TIME_DONE"
    RBF=$BUILD_NAME.rbf

    mkdir -p "$BUILD_OUTPUT_DIR"
    cp "${work_dir}/output_files/$rev.rbf" "$BUILD_OUTPUT_DIR/$RBF"

    for file in ${work_dir}/output_files/*rpt ${work_dir}/output_files/*summary; do
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
fi

echo ""
printf "%s %02d:%02d:%02d\n" "Total Build Time:" "$(($SECONDS / 3600))" "$((($SECONDS / 60) % 60))" "$(($SECONDS % 60))"
echo ""

# Delete empty SOPC directories in the user's home directory
find ~ -maxdepth 1 -type d -empty -iname "sopc_altera_pll*" -delete
