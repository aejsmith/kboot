#!/usr/bin/env python
#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

import sys, os
from subprocess import Popen, PIPE

if len(sys.argv) > 1 and (sys.argv[1] == "--help" or sys.argv[1] == "-h"):
    print("Usage: %s [<path to kboot.elf> <path to addr2line>] << <output>" % (sys.argv[0]))
    print()
    print("Input the backtrace output from KBoot (after the 'Backtrace' line).")
    print()
    print("If no arguments are specified, then this will assume it is run from the root")
    print("of the source directory, with .options.cache matching the configuration that")
    print("the backtrace was from and the build tree containing the kboot.elf the trace")
    print("came from.")
    sys.exit(1)

if len(sys.argv) == 1:
    exec(open(".options.cache").read())
    loader = os.path.join('build', CONFIG, 'source', 'kboot.elf')
    addr2line = CROSS_COMPILE + 'addr2line'
else:
    if len(sys.argv) != 3:
        print("Invalid arguments")
        sys.exit(1)
    loader = sys.argv[1]
    addr2line = sys.argv[2]

lines = sys.stdin.readlines()
for line in lines:
    if not line.startswith(" 0x"):
        continue

    line = line.strip().split()
    if len(line) == 2:
        if not line[1].startswith("(0x"):
            continue
        addr = line[1][1:-1]
    elif len(line) == 1:
        addr = line[0]
    else:
        continue

    process = Popen([addr2line, '-f', '-e', loader, addr], stdout = PIPE, stderr = PIPE)
    output = process.communicate()[0].decode("utf-8").split('\n')
    if process.returncode != 0:
        continue

    print('%s - %s @ %s' % (addr, output[0], output[1]))
