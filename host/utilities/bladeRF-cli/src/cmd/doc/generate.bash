#!/usr/bin/env bash
#
# @file generate.bash
#
# @brief Generates interactive documentation for bladeRF-cli
#
# This file is part of the bladeRF project
#
# Copyright (C) 2014-2016 Rey Tucker <bladerf@reytucker.us>
# Copyright (C) 2014-2018 Nuand LLC
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# We use bashisms, so we need actual bash
[ -z "${BASH_VERSION}" ] && echo "This script requires bash." && exit 2

# My command name
MYNAME="${0##*/}"

# Set our defaults
[ -z "${AWK_CMD}" ]      && AWK_CMD=$(type -p awk)
[ -z "${DATE_CMD}" ]     && DATE_CMD=$(type -p date)
[ -z "${GREP_CMD}" ]     && GREP_CMD=$(type -p grep)
[ -z "${HOSTNAME_CMD}" ] && HOSTNAME_CMD=$(type -p hostname)
[ -z "${PANDOC_CMD}" ]   && PANDOC_CMD=$(type -p pandoc)
[ -z "${SED_CMD}" ]      && SED_CMD=$(type -p sed)
[ -z "${WHOAMI_CMD}" ]   && WHOAMI_CMD=$(type -p whoami)

# If we can do so, generate a user@host string to indicate who built this.
if [[ ! -z "${WHOAMI_CMD}" && ! -z "${HOSTNAME_CMD}" ]]; then
    [ -z "${BUILDER}" ] && BUILDER="$(${WHOAMI_CMD})@$(${HOSTNAME_CMD})"
else
    BUILDER="UNKNOWN"
fi

# Prints usage string
print_usage(){
    echo "usage: ${MYNAME} [--options...] inputfile.md"
}

# Prints help
print_help(){
    print_usage
    echo ""
    echo "Options:"
    echo "  --awk=AWK_CMD        Path to awk (default: ${AWK_CMD})"
    echo "  --date=DATE_CMD      Path to date (default: ${DATE_CMD})"
    echo "  --grep=GREP_CMD      Path to grep (default: ${GREP_CMD})"
    echo "  --pandoc=PANDOC_CMD  Path to pandoc (default: ${PANDOC_CMD})"
    echo "  --sed=SED_CMD        Path to sed (default: ${SED_CMD})"
    echo "  --builder=BUILDER    Builder identification (default: ${BUILDER})"
    echo "  --test               Ensure we can run properly, then exit"
    echo "  --verbose            Print additional debugging information"
    echo "  --help               Print this help"
}

# Prints command-not-found error
notfound(){
    echo "${MYNAME}: Can't find \"$1\" command (try setting --$1=/path/to/$1)"
    exit 2
}

# Parse command line arguments...
while [[ "$#" > 0 ]]; do
    case "$1" in
        --awk=*)        AWK_CMD="${1#*=}";      shift 1;;
        --date=*)       DATE_CMD="${1#*=}";     shift 1;;
        --grep=*)       GREP_CMD="${1#*=}";     shift 1;;
        --pandoc=*)     PANDOC_CMD="${1#*=}";   shift 1;;
        --sed=*)        SED_CMD="${1#*=}";      shift 1;;
        --builder=*)    BUILDER="${1#*=}";      shift 1;;

        --test)         TESTONLY=1;             shift 1;;
        --verbose)      VERBOSE=1;              shift 1;;
        --help)         print_help;             exit 0;;

        --awk|--builder|--date|--grep|--pandoc|--sed)
            echo "$1 requires an argument" >&2;
            exit 1;;

        -*)
            echo "unknown option: $1" >&2;
            exit 1;;

        *)
            INPUT_FILE="$1";
            shift 1;;
    esac
done

[ -n "${VERBOSE}" ] && echo "${MYNAME}: Verbose output enabled!"

# Check input file paths
if [ -z "${INPUT_FILE}" ]; then
    print_usage
    exit 1
fi

if [ ! -r "${INPUT_FILE}" ]; then
    echo "${MYNAME}: Can't read ${INPUT_FILE}"
    exit 2
fi

# Derive our current directory
INPUT_DIR="${INPUT_FILE%/*}"

# Set INPUT_DIR to "." if we're in the current directory
[ "${INPUT_FILE}" == "${INPUT_DIR}" ] && INPUT_DIR="."

# Check for valid directory
if [ ! -d "${INPUT_DIR}" ]; then
    echo "${MYNAME}: Can't find directory ${INPUT_DIR}"
    exit 2
fi

# Test for necessary commands
[ ! -x "${AWK_CMD}" ]    && notfound awk
[ ! -x "${DATE_CMD}" ]   && notfound date
[ ! -x "${GREP_CMD}" ]   && notfound grep
[ ! -x "${PANDOC_CMD}" ] && notfound pandoc
[ ! -x "${SED_CMD}" ]    && notfound sed

# For the cmd_help.h header
COPYRIGHT_YEAR="$(${DATE_CMD} +"%Y")"
GENERATED="$(${DATE_CMD}) by ${BUILDER}"

# Print out settings if verbose
if [ -n "${VERBOSE}" ]; then
    echo "${MYNAME}: *** BEGIN VARIABLE DUMP ***"
    echo "${MYNAME}: Behaviour flags:"
    echo "${MYNAME}:    TESTONLY        ${TESTONLY}"
    echo "${MYNAME}:    VERBOSE         ${VERBOSE}"
    echo "${MYNAME}: Command paths:"
    echo "${MYNAME}:    AWK_CMD         ${AWK_CMD}"
    echo "${MYNAME}:    DATE_CMD        ${DATE_CMD}"
    echo "${MYNAME}:    GREP_CMD        ${GREP_CMD}"
    echo "${MYNAME}:    PANDOC_CMD      ${PANDOC_CMD}"
    echo "${MYNAME}:    SED_CMD         ${SED_CMD}"
    echo "${MYNAME}: Files:"
    echo "${MYNAME}:    INPUT_FILE      ${INPUT_FILE}"
    echo "${MYNAME}:    INPUT_DIR       ${INPUT_DIR}"
    echo "${MYNAME}: Build Identification:"
    echo "${MYNAME}:    BUILDER         ${BUILDER}"
    echo "${MYNAME}:    COPYRIGHT_YEAR  ${COPYRIGHT_YEAR}"
    echo "${MYNAME}:    GENERATED       ${GENERATED}"
    echo "${MYNAME}: *** END VARIABLE DUMP ***"
fi

# We made it this far, we're okay
[ ! -z "${TESTONLY}" ] && exit 0

# Start generating documentation
echo "${MYNAME}: Generating HTML doc (cmd_help.html)..."
${PANDOC_CMD} \
    --standalone --self-contained --toc -f markdown -t html5 \
    --metadata title="bladeRF-cli commands" -o cmd_help.html.new \
    ${INPUT_FILE} \
    && mv cmd_help.html.new cmd_help.html

if [ ! -s "cmd_help.html" ]; then
    echo "${MYNAME}: Failed to generate HTML help!"
fi

echo "${MYNAME}: Generating man page snippet (cmd_help.man)..."
${PANDOC_CMD} \
    -f markdown -t man -o cmd_help.man.new ${INPUT_FILE} \
    && mv cmd_help.man.new cmd_help.man

if [ ! -s "cmd_help.man" ]; then
    echo "${MYNAME}: Failed to generate man page! Using pre-built file."
    cp ${INPUT_DIR}/cmd_help.man.in cmd_help.man
fi

echo "${MYNAME}: Generating text version (cmd_help.txt)..."
${PANDOC_CMD} \
    --ascii --columns=70 -f markdown -t plain -o cmd_help.txt.new \
    ${INPUT_FILE} \
    && mv cmd_help.txt.new cmd_help.txt

if [ ! -s "cmd_help.txt" ]; then
    echo "${MYNAME}: Failed to generate text version! Using pre-built file."
    cp ${INPUT_DIR}/cmd_help.h.in cmd_help.h
    exit 0
fi

echo "${MYNAME}: Generating include file (cmd_help.h)..."

# Add a couple newlines to the end, to give the REPL a chance to empty itself.
echo >> cmd_help.txt
echo >> cmd_help.txt

# Heading!
${AWK_CMD} '{print}' > cmd_help.h.new <<EOF
/**
 * @file cmd_help.h
 *
 * @brief Autogenerated interactive CLI help
 *
 * This file is part of the bladeRF project
 *
 * Copyright (C) 2014-${COPYRIGHT_YEAR} Nuand LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * WARNING: THIS FILE IS AUTOMATICALLY GENERATED BY GENERATE.BASH.
 * Any manual modifications to this file will be overwritten!
 * To edit the content of help strings, please edit ${INPUT_FILE##*/}
 *
 * Last generated: ${GENERATED}
 */

#ifndef BLADERF_CLI_DOC_CMD_HELP_H__
#define BLADERF_CLI_DOC_CMD_HELP_H__
EOF

# Lines with command-words alone that are to be removed from stream, are
# built up as a string of regexps for grep to use.
REMOVE_PATT=$(
    ${GREP_CMD} -E '^Usage' cmd_help.txt | \
    ${AWK_CMD} '
        {
            printf("|^%s$",$2);
        }
        END{
            printf("\n")
        }' | \
    ${SED_CMD} -e 's/.//')

${GREP_CMD} -vEe "${REMOVE_PATT}" cmd_help.txt | \
    ${GREP_CMD} -vE '^-{2,}$' | \
    ${SED_CMD} -e 's/"/\\"/g' | \
    ${GREP_CMD} -Ev '^\[.*\]$' | \
    ${AWK_CMD} '
        # Remove more than one consecutive empty line
        BEGIN{empty=1}
        {
            if ($1==""){
                empty++;
                if (empty<=1){
                    print $0;
                }
            } else {
                empty=0;
                print $0
            }
        } ' | \
    ${AWK_CMD} '
        # Main code generator engine
        /^Usage:/{
            printf("\n\n#define CLI_CMD_HELPTEXT_%s \\\n",$2);
        }
        {
            print "  \""$0"\\n\" \\"
        }
    ' >> cmd_help.h.new

# We end with a \, so throw an extra line in there to keep the preprocessor
# from exploding.  Also close out our wrapper.
echo >> cmd_help.h.new
echo "#endif // BLADERF_CLI_VERSION_H__" >> cmd_help.h.new

mv cmd_help.h.new cmd_help.h
