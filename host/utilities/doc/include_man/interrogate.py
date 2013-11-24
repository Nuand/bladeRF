#!/usr/bin/env python
"""
This file is part of the bladeRF project

bladeRF-cli help crawler
Copyright (C) 2013 Nuand LLC

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
"""

import os
from subprocess import Popen, PIPE
import sys


def run_command(exe, cmd):
    """Returns the output from running a command in interactive mode"""
    realcmd = "\n%s\nquit\n" % cmd
    out = Popen([exe, "-i"], stdin=PIPE, stdout=PIPE, stderr=PIPE
                ).communicate(input=realcmd)[0]
    return out


def parse_help(txt):
    """Splits a two-column help listing into command and description,
       returning a list of tuples."""
    lines = txt.splitlines()
    out = []
    for l in lines:
        l = l.strip().split(' ', 1)
        if len(l) == 2:
            cmd, desc = l
            desc = desc.strip()
            out += [(cmd, desc)]

    return out

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("usage: %s <bladeRF-cli executable>" % sys.argv[0])
        sys.exit(1)
    target = sys.argv[1]

    print("[INTERACTIVE COMMANDS]")

    if os.path.basename(target) in ['bladeRF-cli']:
        # Only do stuff if the command is known to have an interactive mode
        for cmd, desc in parse_help(run_command(target, 'help')):
            first = True
            print(r".PP")
            print(r"\fB%s\fP - %s" % (cmd, desc))
            print(r".RS 4")
            for line in run_command(target, 'help %s' % cmd).splitlines():
                if len(line) is 0:
                    print(".PP")
                else:
                    if first:
                        print(r"Syntax: \fB%s\fP" % line)
                        first = False
                    else:
                        print(line)
            print(r".RE")
    else:
        print("%s does not support an interactive mode." % (
              os.path.basename(target)))
