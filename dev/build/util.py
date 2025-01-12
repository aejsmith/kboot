#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

import os, SCons.Errors
from SCons.Script import *
from functools import reduce

# Helper for creating source lists with certain files only enabled by config
# settings.
def FeatureSources(config, files):
    output = []
    for f in files:
        if type(f) is tuple:
            if reduce(lambda x, y: x or y, [config[x] for x in f[0:-1]]):
                output.append(File(f[-1]))
        else:
            output.append(File(f))
    return output

# Raise a stop error.
def StopError(str):
    # Don't break if the user is trying to get help.
    if GetOption('help'):
        Return()
    else:
        raise SCons.Errors.StopError(str)

# Test if a program exists by looking it up in the path.
def which(program):
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ['PATH'].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None
