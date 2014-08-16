#!/usr/bin/python
#
################################################################################
# Plutocracy - Copyright (C) 2008 - Michael Levin
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
################################################################################

import glob, os, sys

# Package parameters
package = 'plutocracy'
version = '0.0.3'

# Platform considerations
windows = sys.platform == 'win32'
darwin = sys.platform == 'darwin'

# Convert a POSIX path to OS-independent
def path(p):
        if windows:
                p = p.replace('/', '\\')
        return p

# Builder for precompiled headers
gch_builder = Builder(action = '$CC $CFLAGS $CCFLAGS $_CCCOMCOM ' +
                               '-x c-header -c $SOURCE -o $TARGET')

# Create a default environment. Have to set environment variables after
# initialization so that SCons doesn't mess them up.
default_env = Environment(ENV = os.environ, BUILDERS = {'GCH' : gch_builder})

# Tells SCons to be smarter about caching dependencies
default_env.SetOption('implicit_cache', 1)

# Adds an option that may have been set via environment flags
def AddEnvOption(env, opts, opt_name, opt_desc, env_name = None, value = None):
        if env_name == None:
                env_name = opt_name;
        if env_name == opt_name:
                opt_desc += ' (environment variable)'
        else:
                opt_desc += ' (environment variable ' + env_name + ')'
        if env_name in os.environ:
                opts.Add(opt_name, opt_desc, os.environ[env_name])
        elif value == None:
                opts.Add(opt_name, opt_desc, env.get(opt_name))
        else:
                opts.Add(opt_name, opt_desc, value)

# Options are loaded from 'custom.py'. Default values are only provided for
# the variables that do not get set by SCons itself.
config_file = ARGUMENTS.get('config', 'custom.py')
opts = Options(config_file)

# First load the mingw variable because it affects default variables
opts.Add(BoolOption('mingw', 'Set to True if compiling with MinGW', False))
opts.Update(default_env)
mingw = default_env.get('mingw')
if mingw:
        default_env = Environment(tools = ['mingw'], ENV = os.environ,
                                  BUILDERS = {'GCH' : gch_builder})

# Now load the rest of the options
AddEnvOption(default_env, opts, 'CC', 'Compiler to use')
AddEnvOption(default_env, opts, 'CFLAGS', 'Compiler flags')
AddEnvOption(default_env, opts, 'LINK', 'Linker to use')
AddEnvOption(default_env, opts, 'LINKFLAGS', 'Linker flags', 'LDFLAGS')
AddEnvOption(default_env, opts, 'PREFIX', 'Installation path prefix',
             'PREFIX', '/usr/local/')
opts.Add(EnumOption('pch', 'Use precompiled headers if possible', 'common',
                    allowed_values = ('no', 'common', 'all')))
opts.Add(BoolOption('libc_errors', 'Generate errors for LIBC misuse', False))
opts.Add(BoolOption('dump_env', 'Dump SCons Environment contents', False))
opts.Add(BoolOption('dump_path', 'Dump path enviornment variable', False))
opts.Update(default_env)
opts.Save(config_file, default_env)

# Windows compiler and linker need some extra flags
if windows:
        if default_env['CC'] == 'cl':
                default_env.PrependUnique(CFLAGS = '/MD')
        if default_env['LINK'] == 'link':
                default_env.PrependUnique(LINKFLAGS = '/SUBSYSTEM:CONSOLE')

# Dump Environment
if default_env['dump_env']:
        print default_env.Dump()
if default_env['dump_path']:
        print default_env['ENV']['PATH']

# Setup the help information
Help('\n' + package.title() + ' ' + version +
""" is built using SCons. See the README file for detailed
instructions.

Options are initialized from the environment first and can then be overridden
using either the command-line or the configuration file. Values only need to
be specified on the command line once and are then saved to the configuration
file. The filename of the configuration file can be specified using the
'config' command-line option:

  scons config='custom.py'

The following options are available:
""")
Help(opts.GenerateHelpText(default_env))

################################################################################
#
# scons plutocracy -- Compile plutocracy
#
################################################################################
plutocracy_src = ([path('src/plutocracy.c')] + glob.glob(path('src/*/*.c')))
plutocracy_env = default_env.Clone()
plutocracy_objlibs = []
if windows:
        plutocracy_src.remove(path('src/common/c_os_posix.c'))
        plutocracy_env.Append(CPPPATH = 'windows/include')
        plutocracy_env.Append(LIBPATH = 'windows/lib')
        plutocracy_env.Append(LIBS = ['SDLmain', 'OpenGL32', 'GlU32',
                                      'user32', 'shell32', 'Ws2_32'])
        if mingw:
                plutocracy_env.ParseConfig('sh sdl-config --prefix=windows' +
                                                        ' --cflags --libs')
                plutocracy_objlibs = [path('windows/lib/zdll.lib'),
                                      path('windows/lib/libpng.lib'),
                                      path('windows/lib/SDL.lib'),
                                      path('windows/lib/SDL_ttf.lib')]
        else:
                plutocracy_env.Append(CPPPATH = ';windows/include/SDL')
                plutocracy_env.Append(LIBS = ['zdll', 'libpng', 'SDL',
                                              'SDLmain', 'SDL_ttf'])
else:
        plutocracy_src.remove(path('src/common/c_os_windows.c'))
        plutocracy_env.Append(CPPPATH = '.')
        plutocracy_env.Append(LIBS = ['SDL_ttf', 'GL', 'GLU', 'z', 'png'])
        plutocracy_env.ParseConfig('sdl-config --cflags --libs')

        # Manually add the framework paths for MacOSX, is there a better way?
        if darwin:
                def DarwinFramework(name, prefix = ''):
                        plutocracy_env.Append(CPPPATH = prefix + '/Library/Frameworks/' +
                                                        name + '.framework/Headers')
                        plutocracy_env.Append(LIBPATH = prefix + '/Library/Frameworks/' +
                                                        name + '.framework/Libraries')
                DarwinFramework('libpng')
                DarwinFramework('SDL')
                DarwinFramework('SDL_ttf')
                DarwinFramework('OpenGL', '/System')

plutocracy_obj = plutocracy_env.Object(plutocracy_src)
plutocracy = plutocracy_env.Program(package, plutocracy_obj +
                                             plutocracy_objlibs)
if windows:
        plutocracy_env.Clean(plutocracy, 'plutocracy.exe.manifest')
Default(plutocracy)

# Build the precompiled headers as dependencies of the main program. We have
# to manually specify the header dependencies because SCons is too stupid to
# scan the header files on its own.
plutocracy_pch = []
def PlutocracyPCH(header, deps = []):
        global plutocracy_pch
        pch = plutocracy_env.GCH(header + '.gch', header)
        plutocracy_env.Depends(pch, deps)
        plutocracy_env.Depends(plutocracy_obj, pch)
        plutocracy_pch += pch
if plutocracy_env['pch'] != 'no' and (not windows or mingw):
        common_deps = glob.glob('src/common/*.h')
        PlutocracyPCH('src/common/c_shared.h', common_deps)
        if plutocracy_env['pch'] == 'all':
                network_deps = common_deps + glob.glob('src/network/*.h')
                PlutocracyPCH('src/network/n_common.h')
                render_deps = common_deps + glob.glob('src/render/*.h')
                PlutocracyPCH('src/render/r_common.h', render_deps)
                interface_deps = (render_deps + glob.glob('src/interface/*.h') +
                                  glob.glob('src/game/*.h'))
                PlutocracyPCH('src/interface/i_common.h', interface_deps)
                PlutocracyPCH('src/game/g_common.h', interface_deps)

# Windows has a prebuilt config
if windows:
        plutocracy_config = 'windows/include/config.h'

# Generate a config.h with definitions
else:
        def WriteConfigH(target, source, env):

                # Get subversion revision and add it to the version
                svn_revision = ''
                try:
                        in_stream, out_stream = os.popen2('svn info')
                        result = out_stream.read()
                        in_stream.close()
                        out_stream.close()
                        start = result.find('Revision: ')
                        if start >= 0:
                                end = result.find('\n', start)
                                svn_revision = 'r' + result[start + 10:end]
                except:
                        pass

                # Write the config
                config = open('config.h', 'w')
                config.write('\n/* Package parameters */\n' +
                             '#define PACKAGE "' + package + '"\n' +
                             '#define PACKAGE_STRING "' + package.title() +
                             ' ' + version + svn_revision + '"\n' +
                             '\n/* Configured paths */\n' +
                             '#define PKGDATADIR "' + install_data + '"\n')

                # LibC function misuse errors
                if plutocracy_env['libc_errors']:
                        config.write('\n/* Plutocracy */\n' +
                                     '#define PLUTOCRACY_LIBC_ERRORS\n')

                # Darwin special define
                if darwin:
                        config.write('\n/* Platform */\n' +
                                     '#ifndef DARWIN\n#define DARWIN\n#endif')

                config.close()

        plutocracy_config = plutocracy_env.Command('config.h', '', WriteConfigH)
        plutocracy_env.Depends(plutocracy_config, 'SConstruct')

plutocracy_env.Depends(plutocracy_obj + plutocracy_pch, plutocracy_config)
plutocracy_env.Depends(plutocracy_config, config_file)

################################################################################
#
# scons install -- Install plutocracy
#
################################################################################
install_prefix = os.path.join(os.getcwd(), plutocracy_env['PREFIX'])
install_data = os.path.join(install_prefix, 'share', 'plutocracy')
default_env.Alias('install', install_prefix)
default_env.Depends('install', plutocracy)

def InstallRecursive(dest, src, exclude = []):
        bad_exts = ['exe', 'o', 'obj', 'gch', 'pch', 'user', 'ncb', 'suo',
                    'manifest']
        files = os.listdir(src)
        for f in files:
                ext = f.rsplit('.')
                ext = ext[len(ext) - 1]
                if (f[0] == '.' or ext in bad_exts):
                        continue
                src_path = os.path.join(src, f)
                if src_path in exclude:
                        continue
                if (os.path.isdir(src_path)):
                        InstallRecursive(os.path.join(dest, f), src_path)
                        continue
                default_env.Install(dest, src_path)

# Files that get installed
default_env.Install(os.path.join(install_prefix, 'bin'), plutocracy)
default_env.Install(install_data, ['AUTHORS', 'ChangeLog', 'CC', 'COPYING',
                                   'README'])
InstallRecursive(os.path.join(install_data, 'gui'), 'gui')
InstallRecursive(os.path.join(install_data, 'models'), 'models')
InstallRecursive(os.path.join(install_data, 'configs'), 'configs')
InstallRecursive(os.path.join(install_data, 'lang'), 'lang')

################################################################################
#
# scons gendoc -- Generate automatic documentation
#
################################################################################
gendoc_env = default_env.Clone()
gendoc_header = path('gendoc/header.txt')
gendoc = gendoc_env.Program(path('gendoc/gendoc'),
                            glob.glob(path('gendoc/*.c')))
if windows:
        gendoc_env.Clean(gendoc, path('gendoc/gendoc.exe.manifest'))

def GendocOutput(output, in_path, title, inputs = []):
        output = os.path.join('gendoc', 'docs', output)
        inputs = (glob.glob(os.path.join(in_path, '*.h')) +
                  glob.glob(os.path.join(in_path, '*.c')) + inputs);
        cmd = gendoc_env.Command(output, inputs, path('gendoc/gendoc') +
                                 ' --file $TARGET --header "' +
                                 gendoc_header + '" --title "' + title + '" ' +
                                 ' '.join(inputs))
        gendoc_env.Depends(cmd, gendoc)
        gendoc_env.Depends(cmd, gendoc_header)

# Gendoc HTML files
GendocOutput('gendoc.html', 'gendoc', 'GenDoc')
GendocOutput('shared.html', path('src/common'), 'Plutocracy Shared',
             [path('src/render/r_shared.h'),
              path('src/interface/i_shared.h'),
              path('src/game/g_shared.h'),
              path('src/network/n_shared.h')] +
             glob.glob(path('src/render/*.c')) +
             glob.glob(path('src/interface/*.c')) +
             glob.glob(path('src/game/*.c')) +
             glob.glob(path('src/network/*.c')))
GendocOutput('render.html', path('src/render'), 'Plutocracy Render')
GendocOutput('interface.html', path('src/interface'),
             'Plutocracy Interface')
GendocOutput('game.html', path('src/game'), 'Plutocracy Game')
GendocOutput('network.html', path('src/network'), 'Plutocracy Network')

################################################################################
#
# scons dist -- Distributing the source package tarball
#
################################################################################

# The compression command depends on the target system
compress_cmd = 'tar -czf '
compress_suffix = '.tar.gz'
if windows:
        compress_cmd = 'windows\\bin\\zip -r '
        compress_suffix = '.zip'

dist_name = package + '-' + version
dist_tarball = dist_name + compress_suffix
default_env.Depends(dist_name, 'gendoc')
default_env.Command(dist_tarball, dist_name,
                    [compress_cmd + dist_tarball + ' ' + dist_name])
default_env.Alias('dist', dist_tarball)
default_env.AddPostAction(dist_tarball, Delete(dist_name))

# Files that get distributed in a source package
dist_dlls = glob.glob('*.dll')
default_env.Install(dist_name, ['AUTHORS', 'ChangeLog', 'CC', 'COPYING',
                                'README', 'SConstruct', 'Makefile',
                                'todo.sh', 'genlang.py', 'find.sh',
                                'export_plum.py', 'check.py'] + dist_dlls)
InstallRecursive(os.path.join(dist_name, 'gendoc'), 'gendoc',
                 [path('gendoc/gendoc')])
InstallRecursive(os.path.join(dist_name, 'gui'), 'gui')
InstallRecursive(os.path.join(dist_name, 'models'), 'models')
InstallRecursive(os.path.join(dist_name, 'configs'), 'configs')
InstallRecursive(os.path.join(dist_name, 'lang'), 'lang')
InstallRecursive(os.path.join(dist_name, 'src'), 'src')
InstallRecursive(os.path.join(dist_name, 'windows'), 'windows',
                 ['windows/vc8/Debug', 'windows/vc8/Release'])

################################################################################
#
# scons release -- Distributing the binary package tarball
#
################################################################################
release_name = dist_name
if windows:
        release_name += '-win32'
else:
        release_name += '-posix'
release_tarball = release_name + compress_suffix
default_env.Depends(release_name, plutocracy)
default_env.Command(release_tarball, release_name,
                    [compress_cmd + release_tarball + ' ' + release_name])
default_env.Alias('release', release_tarball)
default_env.AddPostAction(release_tarball, Delete(release_name))

# Files that get distributed in a binary release
default_env.Install(release_name, ['AUTHORS', 'ChangeLog', 'CC', 'COPYING',
                                   'README', plutocracy])
InstallRecursive(os.path.join(release_name, 'gui'), 'gui')
InstallRecursive(os.path.join(release_name, 'models'), 'models')
InstallRecursive(os.path.join(release_name, 'configs'), 'configs')
InstallRecursive(os.path.join(release_name, 'lang'), 'lang')
if windows:
        default_env.Install(release_name, dist_dlls)

