#
# Copyright (C) 2014-2015 Alex Smith
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

# Version number.
version = 20151220

# Build flags for both host and target.
build_flags = {
    'CCFLAGS': [
        '-Wall', '-Wextra', '-Wno-variadic-macros', '-Wno-unused-parameter',
        '-Wwrite-strings', '-Wmissing-declarations', '-Wredundant-decls',
        '-Wno-format', '-Werror', '-Wno-error=unused', '-pipe',
        '-Wno-error=unused-function',
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

# Build flags for host.
host_flags = {
    'CPPDEFINES': {
        'KBOOT_PREFIX': '\\"${PREFIX}\\"',
        'KBOOT_LIBDIR': '\\"${LIBDIR}\\"',
        'KBOOT_LOADER_VERSION': '\\"${VERSION}\\"',
    }
}

# Build flags for target.
target_flags = {
    'CCFLAGS': [
        '-gdwarf-2', '-pipe', '-nostdlib', '-nostdinc', '-ffreestanding',
        '-fno-stack-protector', '-Os', '-fno-omit-frame-pointer',
        '-fno-optimize-sibling-calls',
    ],
    'ASFLAGS': ['-nostdinc'],
    'LINKFLAGS': ['-nostdlib', '-Wl,--build-id=none'],
}

###############
# Build setup #
###############

import os, sys
from subprocess import Popen, PIPE

sys.path = [os.path.abspath(os.path.join('dev', 'build'))] + sys.path
import util, vcs

# Get revision.
revision = vcs.revision_id()

# Compile for debug by default if building from git.
debug_default = 1 if revision is not None else 0

# Configurable build options.
opts = Variables('.options.cache')
opts.AddVariables(
    ('CONFIG', 'Target system configuration name.'),
    ('CROSS_COMPILE', 'Cross compiler tool prefix (prepended to all tool names).', ''),
    ('PREFIX', 'Installation prefix.', '/usr/local'),
    BoolVariable('DEBUG', 'Whether to compile with debugging features.', debug_default),
)

# Create the build environment.
env = Environment(ENV = os.environ, variables = opts, tools = ['default', 'textfile'])
opts.Save('.options.cache', env)

# Define the version string.
env['VERSION'] = '%d+%s' % (version, revision) if revision is not None else '%d' % (version)

# Get a list of known configurations.
configs = SConscript('config/SConscript')

# Generate help text.
helptext = \
    'To build KBoot, a target system configuration must be specified on the command\n' + \
    'line with the CONFIG option. The following configurations are available:\n' + \
    '\n' + \
    ''.join(['  %-12s - %s\n' % (c, configs[c]['description']) for c in sorted(configs.iterkeys())]) + \
    '\n' + \
    'The following build options can be set on the command line. These will be saved\n' + \
    'for later invocations of SCons, so you do not need to specify them every time:\n' + \
    opts.GenerateHelpText(env) + \
    '\n' + \
    'For information on how to build KBoot, please refer to documentation/readme.txt.\n'
Help(helptext)

# Check if the configuration specified is invalid.
if not env.has_key('CONFIG'):
    util.StopError("No target system configuration specified. See 'scons -h'.")
elif not env['CONFIG'] in configs:
    util.StopError("Unknown configuration '%s'." % (env['CONFIG']))

# Make build output nice.
verbose = ARGUMENTS.get('V') == '1'
def compile_str(msg):
    return '\033[0;32m%8s\033[0m $TARGET' % (msg)
def compile_str_func(msg, target, source, env):
    return '\033[0;32m%8s\033[0m %s' % (msg, str(target[0]))

if not verbose:
    env['INSTALLSTR'] = compile_str('INSTALL')

    # Substfile doesn't provide a method to override the output. Hack around.
    env['BUILDERS']['Substfile'].action.strfunction = lambda t, s, e: compile_str_func('GEN', t, s, e)

# Merge in build flags.
for (k, v) in build_flags.items():
    env[k] = v

# Add a builder to preprocess linker scripts.
env['BUILDERS']['LDScript'] = Builder(action = Action(
    '$CC $_CCCOMCOM $ASFLAGS -E -x c $SOURCE | grep -v "^\#" > $TARGET',
    '$GENCOMSTR'))

# Define installation paths.
env['BINDIR'] = os.path.join(env['PREFIX'], 'bin')
env['LIBDIR'] = os.path.join(env['PREFIX'], 'lib', 'kboot')
env['TARGETDIR'] = os.path.join(env['LIBDIR'], env['CONFIG'])

# Define real installation paths taking DESTDIR into account.
env['DESTDIR'] = ARGUMENTS.get('DESTDIR', '/')
env['DESTBINDIR'] = os.path.join(env['DESTDIR'], os.path.relpath(env['BINDIR'], '/'))
env['DESTLIBDIR'] = os.path.join(env['DESTDIR'], os.path.relpath(env['LIBDIR'], '/'))
env['DESTTARGETDIR'] = os.path.join(env['DESTDIR'], os.path.relpath(env['TARGETDIR'], '/'))

################################
# Host build environment setup #
################################

# Set up the host build environment.
host_env = env.Clone()

# Make build output nice.
if not verbose:
    host_env['ARCOMSTR']     = compile_str('HOSTAR')
    host_env['ASCOMSTR']     = compile_str('HOSTAS')
    host_env['ASPPCOMSTR']   = compile_str('HOSTAS')
    host_env['CCCOMSTR']     = compile_str('HOSTCC')
    host_env['CXXCOMSTR']    = compile_str('HOSTCXX')
    host_env['LINKCOMSTR']   = compile_str('HOSTLINK')
    host_env['RANLIBCOMSTR'] = compile_str('HOSTRL')
    host_env['GENCOMSTR']    = compile_str('HOSTGEN')
    host_env['STRIPCOMSTR']  = compile_str('HOSTSTRIP')

# Merge in build flags.
for (k, v) in host_flags.items():
    if host_env.has_key(k):
        if type(v) == dict:
            host_env[k].update(v)
        else:
            host_env[k] += v
    else:
        host_env[k] = v

# Add compiler-specific flags.
output = Popen([host_env['CC'], '--version'], stdout=PIPE, stderr=PIPE).communicate()[0].strip()
host_env['IS_CLANG'] = output.find('clang') >= 0
if host_env['IS_CLANG']:
    for (k, v) in clang_flags.items():
        host_env[k] += v
else:
    for (k, v) in gcc_flags.items():
        host_env[k] += v

# We place the final output binaries in a single directory.
host_env['OUTDIR'] = Dir('build/host/bin')

# Build host system utilities.
SConscript('utilities/SConscript',
    variant_dir = os.path.join('build', 'host', 'utilities'),
    exports = {'env': host_env})

# Create an alias to build the utilities.
utilities = host_env.Glob('${OUTDIR}/*')
Alias('utilities', utilities)

# Create installation targets for the utilities.
for target in utilities:
    install = host_env.Install(host_env['DESTBINDIR'], target)
    Alias('install-utilities', install)

##################################
# Target build environment setup #
##################################

config = configs[env['CONFIG']]['config']

# Set the debug flag in the configuration.
if env['DEBUG']:
    config['DEBUG'] = True

# Make build output nice.
if not verbose:
    env['ARCOMSTR']     = compile_str('AR')
    env['ASCOMSTR']     = compile_str('AS')
    env['ASPPCOMSTR']   = compile_str('AS')
    env['CCCOMSTR']     = compile_str('CC')
    env['CXXCOMSTR']    = compile_str('CXX')
    env['LINKCOMSTR']   = compile_str('LINK')
    env['RANLIBCOMSTR'] = compile_str('RL')
    env['GENCOMSTR']    = compile_str('GEN')
    env['STRIPCOMSTR']  = compile_str('STRIP')

# Detect which compiler to use.
def guess_compiler(name):
    if os.environ.has_key(name):
        compilers = [os.environ[name]]
    else:
        compilers = [env['CROSS_COMPILE'] + x for x in ['cc', 'gcc', 'clang']]
    for compiler in compilers:
        if util.which(compiler):
            return compiler
    util.StopError('Toolchain has no usable compiler available.')
if os.environ.has_key('CC') and os.path.basename(os.environ['CC']) == 'ccc-analyzer':
    # Support the clang static analyzer.
    env['CC'] = os.environ['CC']
    env['ENV']['CCC_CC'] = guess_compiler('CCC_CC')
else:
    env['CC'] = guess_compiler('CC')

# Set paths to other build utilities.
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
output = Popen([env['CC'], '--version'], stdout=PIPE, stderr=PIPE).communicate()[0].strip()
env['IS_CLANG'] = output.find('clang') >= 0
if env['IS_CLANG']:
    for (k, v) in clang_flags.items():
        env[k] += v
else:
    for (k, v) in gcc_flags.items():
        env[k] += v

# Add the compiler include directory for some standard headers.
incdir = Popen([env['CC'], '-print-file-name=include'], stdout=PIPE).communicate()[0].strip()
env['CCFLAGS'] += ['-isystem%s' % (incdir)]
env['ASFLAGS'] += ['-isystem%s' % (incdir)]

# Change the Decider to MD5-timestamp to speed up the build a bit.
Decider('MD5-timestamp')

# We place the final output binaries in a single directory.
env['OUTDIR'] = Dir('build/%s/bin' % (env['CONFIG']))

# Build the loader itself.
SConscript('source/SConscript',
    variant_dir = os.path.join('build', env['CONFIG'], 'source'),
    exports = ['config', 'env'])

# Default targets are all output binaries.
targets = env.Glob('${OUTDIR}/*')
Default(Alias('loader', targets))

# Install all files in the output directory.
for target in targets:
    install = env.Install(env['DESTTARGETDIR'], target)
    Alias('install', install)

# Add a debug install target to install the debug binary.
install = env.Install(env['DESTTARGETDIR'], env['KBOOT'])
Alias('install-debug', install)

################
# Test targets #
################

SConscript('test/SConscript',
    variant_dir = os.path.join('build', env['CONFIG'], 'test'),
    exports = ['config', 'env'])

# Get QEMU script to run.
qemu = ARGUMENTS.get('QEMU', '')
if len(qemu):
    qemu = '%s-%s.sh' % (env['CONFIG'], qemu)
else:
    qemu = '%s.sh' % (env['CONFIG'])

# Add a target to run the test script for this configuration (if it exists).
script = os.path.join('test', 'qemu', qemu)
if os.path.exists(script):
    Alias('qemu', env.Command('__qemu', ['loader', 'test', 'utilities'], Action(script, None)))
