#!/bin/bash
# Runs the --help-interactive command and formats the output for help2man

TAB=$'\t'

if [ -z "$1" ]
then
    echo "usage: $0 path_to_executable"
    exit 1
fi

TMPFILE=$(tempfile)
$1 --help-interactive > $TMPFILE

echo "[INTERACTIVE COMMANDS]"

OIFS=$IFS
IFS="
"

IN_RS=0

while read -r line
do
    if [ $(expr match "$line" "COMMAND:") -eq 8 ]
    then
        # The line starts with "COMMAND:", so it's the usage summary for
        # the command.
        subline=${line##COMMAND: }
        cmdname=$(echo "$subline" | cut -d' ' -f1)
        restofl=$(echo "$subline" | cut -d' ' -f2-)
        if [ "${cmdname}" == "${restofl}" ]
        then
            # no arguments, so cut got confused
            restofl=""
        fi
        echo ".SS \fB${cmdname}\fR ${restofl}"
    else
        # How many tabs at the start?
        trimmed=$line
        tab_count=0
        while [ $(expr match "$trimmed" "$TAB") -eq 1 ]
        do
            let tab_count++
            trimmed=$(echo "$trimmed" | sed 's/^\t//')
        done
        #echo "DEBUG: tabs $tab_count"

        while [ $IN_RS -lt $tab_count ]
        do
            echo ".RS"
            let IN_RS++
        done

        while [ $IN_RS -gt $tab_count ]
        do
            echo ".RE"
            let IN_RS--
        done

        # embolden words in <>
        trimmed=$(echo "$trimmed" | sed 's/<\([^>]*\)>/\\fB<\1>\\fR/g')

        # check for two-column pair
        first_word=$(echo "$trimmed" | cut -f1)
        rest_of_line=$(echo "$trimmed" | cut -f2-)
        #echo "DEBUG: $trimmed"
        if [ "$first_word" == "$rest_of_line" ]
        then
            # it's just a single column
            echo "${first_word}"
        else
            # multiple columns
            echo "${first_word}"
            echo ".RS"
            echo "${rest_of_line}"
            echo ".RE"
        fi
    fi
done < $TMPFILE
IFS=$OIFS

rm -f $TMPFILE
