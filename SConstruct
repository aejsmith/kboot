#
# Copyright (C) 2014 Alex Smith
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

# Build flags for both host and target.
build_flags = {
    'CCFLAGS': [
        '-Wall', '-Wextra', '-Wno-variadic-macros', '-Wno-unused-parameter',
        '-Wwrite-strings', '-Wmissing-declarations', '-Wredundant-decls',
        '-Wno-format', '-Werror', '-Wno-error=unused', '-pipe',
    ],
    'CFLAGS': ['-std=gnu99'],
    'ASFLAGS': ['-D__ASM__'],
}

# GCC-specific build flags.
gcc_flags = {
    'CCFLAGS': ['-Wno-unused-but-set-variable'],
}

# Clang-specific build flags.
clang_flags = {
    # Clang's integrated assembler doesn't support 16-bit code.
    'ASFLAGS': ['-no-integrated-as'],
}

# Build flags for target.
target_flags = {
    'CCFLAGS': [
        '-gdwarf-2', '-pipe', '-nostdlib', '-nostdinc', '-ffreestanding',
        '-fno-stack-protector', '-Os', '-fno-omit-frame-pointer',
    ],
    'ASFLAGS': ['-nostdinc'],
    'LINKFLAGS': ['-nostdlib', '-Wl,--build-id=none'],
}

###############
# Build setup #
###############

import os, sys
from subprocess import Popen, PIPE

sys.path = [os.path.abspath(os.path.join('utilities', 'build'))] + sys.path
import util

# Configurable build options.
opts = Variables('.options.cache')
opts.AddVariables(
    ('CONFIG', 'Target system configuration name.'),
    ('CROSS_COMPILE', 'Cross compiler tool prefix (prepended to all tool names).', ''),
)

# Create the build environment.
env = Environment(ENV = os.environ, variables = opts)
opts.Save('.options.cache', env)

# Get a list of known configurations.
configs = SConscript('config/SConscript')

# Generate help text.
helptext  = 'To build KBoot, a target system configuration must be specified on the command\n'
helptext += 'line with the CONFIG option. The following configurations are available:\n'
helptext += '\n'
for name in sorted(configs.iterkeys()):
    helptext += '  %-12s - %s\n' % (name, configs[name]['description'])
helptext += '\n'
helptext += 'The following build options can be set on the command line. These will be saved\n'
helptext += 'for later invocations of SCons, so you do not need to specify them every time:\n'
helptext += opts.GenerateHelpText(env)
helptext += '\n'
helptext += 'For information on how to build KBoot, please refer to documentation/readme.txt.\n'
Help(helptext)

# Make the output nice.
verbose = ARGUMENTS.get('V') == '1'
if not verbose:
    def compile_str(msg, name):
        return ' \033[0;32m%-6s\033[0m %s' % (msg, name)
    env['ARCOMSTR']     = compile_str('AR', '$TARGET')
    env['ASCOMSTR']     = compile_str('ASM', '$SOURCE')
    env['ASPPCOMSTR']   = compile_str('ASM', '$SOURCE')
    env['CCCOMSTR']     = compile_str('CC', '$SOURCE')
    env['CXXCOMSTR']    = compile_str('CXX', '$SOURCE')
    env['LINKCOMSTR']   = compile_str('LINK', '$TARGET')
    env['RANLIBCOMSTR'] = compile_str('RANLIB', '$TARGET')
    env['GENCOMSTR']    = compile_str('GEN', '$TARGET')
    env['STRIPCOMSTR']  = compile_str('STRIP', '$TARGET')

# Merge in build flags.
for (k, v) in build_flags.items():
    env[k] = v

# Add a builder to preprocess linker scripts.
env['BUILDERS']['LDScript'] = Builder(action = Action(
    '$CC $_CCCOMCOM $ASFLAGS -E -x c $SOURCE | grep -v "^\#" > $TARGET',
    '$GENCOMSTR'))

################################
# Host build environment setup #
################################

# Set up the host build environment.
host_env = env.Clone()

# Add compiler-specific flags.
output = Popen([host_env['CC'], '--version'], stdout=PIPE, stderr=PIPE).communicate()[0].strip()
host_env['IS_CLANG'] = output.find('clang') >= 0
if host_env['IS_CLANG']:
    for (k, v) in clang_flags.items():
        host_env[k] += v
else:
    for (k, v) in gcc_flags.items():
        host_env[k] += v

# Build host system utilities.
#SConscript('utilities/SConscript',
#    variant_dir = os.path.join('build', 'host'),
#    exports = {'env': host_env})

##################################
# Target build environment setup #
##################################

# Check if the configuration specified is invalid.
if not env.has_key('CONFIG'):
    util.StopError("No target system configuration specified. See 'scons -h'.")
elif not env['CONFIG'] in configs:
    util.StopError("Unknown configuration '%s'." % (env['CONFIG']))

config = configs[env['CONFIG']]['config']

# Detect which compiler to use.
compilers = ['cc', 'gcc', 'clang']
compiler = None
for name in compilers:
    path = env['CROSS_COMPILE'] + name
    if util.which(path):
        compiler = path
        break
if not compiler:
    util.StopError('Toolchain has no usable compiler available.')

# Set paths to the various build utilities. The stuff below is to support use
# of clang's static analyzer.
if os.environ.has_key('CC') and os.path.basename(os.environ['CC']) == 'ccc-analyzer':
    env['CC'] = os.environ['CC']
    env['ENV']['CCC_CC'] = compiler
else:
    env['CC'] = compiler
env['AS']      = env['CROSS_COMPILE'] + 'as'
env['OBJDUMP'] = env['CROSS_COMPILE'] + 'objdump'
env['READELF'] = env['CROSS_COMPILE'] + 'readelf'
env['NM']      = env['CROSS_COMPILE'] + 'nm'
env['STRIP']   = env['CROSS_COMPILE'] + 'strip'
env['AR']      = env['CROSS_COMPILE'] + 'ar'
env['RANLIB']  = env['CROSS_COMPILE'] + 'ranlib'
env['OBJCOPY'] = env['CROSS_COMPILE'] + 'objcopy'
env['LD']      = env['CROSS_COMPILE'] + 'ld'

# Override default assembler - it uses as directly, we want to use GCC.
env['ASCOM'] = '$CC $_CCCOMCOM $ASFLAGS -c -o $TARGET $SOURCES'

# Merge in build flags.
for (k, v) in target_flags.items():
    env[k] += v

# Add compiler-specific flags.
output = Popen([compiler, '--version'], stdout=PIPE, stderr=PIPE).communicate()[0].strip()
env['IS_CLANG'] = output.find('clang') >= 0
if env['IS_CLANG']:
    for (k, v) in clang_flags.items():
        env[k] += v
else:
    for (k, v) in gcc_flags.items():
        env[k] += v

# Add the compiler include directory for some standard headers.
incdir = Popen([env['CC'], '-print-file-name=include'], stdout=PIPE).communicate()[0].strip()
env['CCFLAGS'] += ['-isystem', incdir]
env['ASFLAGS'] += ['-isystem', incdir]

# Change the Decider to MD5-timestamp to speed up the build a bit.
Decider('MD5-timestamp')

# Don't use the Default function for compatibility with the main Kiwi
# build system.
defaults = []

SConscript('source/SConscript',
    variant_dir = os.path.join('build', env['CONFIG']),
    exports = ['config', 'defaults', 'env'])

Default(defaults)

###############
# QEMU target #
###############

# Add a target to run the test script for this configuration (if it exists).
script = os.path.join('utilities', 'test', 'test-%s.sh' % (env['CONFIG']))
if os.path.exists(script):
    Alias('qemu', env.Command('__qemu', defaults, Action(script, None)))
