#!/usr/bin/env python
#
# Copyright (C) 2015 Alex Smith
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

import sys
from subprocess import Popen, PIPE

if len(sys.argv) != 2 and len(sys.argv) != 3:
    print('Usage: %s <path to kboot.elf> [<path to addr2line>] << <output>' % (sys.argv[0]))
    sys.exit(1)

loader = sys.argv[1]
if len(sys.argv) == 3:
    addr2line = sys.argv[2]
else:
    addr2line = 'addr2line'

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
