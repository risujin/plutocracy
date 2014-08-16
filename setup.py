from glob import glob
from distutils.core import setup, Extension, Command
from distutils.spawn import spawn
from os.path import join
from subprocess import Popen, PIPE
import distutils.ccompiler, distutils.log
import sys, os
import shelve

d = shelve.open(".d")

git_hash = Popen(["git log --pretty=format:%h -n 1"],
                 stdout=PIPE, shell=True).communicate()[0]

prefix = sys.exec_prefix

PACKAGE_STRING = "Plutocracy 0.0.git(%s)" % git_hash
PKGDATADIR = prefix + "/share/plutocracy"

common_src = glob("src/common/*.c")
game_src = glob("src/game/*.c")
interface_src = glob("src/interface/*.c")
render_src = glob("src/render/*.c")
network_src = glob("src/network/*.c")
api_src = glob("src/api/*.c")

headers = glob("src/*/*.h")

if os.name == "posix":
    common_src.remove("src/common/c_os_windows.c")

plutocracy_src = common_src + game_src + interface_src + render_src + \
                 network_src + api_src

extensions = []

# Check header modification time

headers_changed = False;

def header_changed(header):
    mtime = os.path.getmtime(header)
    if not d.has_key(header):
        d[header] = mtime
        return True
    if d[header] != mtime:
        d[header] = mtime
        return True
    
for header in headers:
    if header_changed(header):
        headers_changed = True
        
api_extension = Extension("plutocracy.api", ["src/plutocracy-lib.c",
                          "src/plutocracy.c"] + plutocracy_src,
                          include_dirs=[".", "/usr/include/SDL",
                                        "/usr/include/pango-1.0",
                                        "/usr/include/glib-2.0",
                                        "/usr/lib/glib-2.0/include"],
                   libraries = ['GL', 'GLU', 'z', 'png', "SDL_Pango",
                                'pango-1.0'],
                   define_macros=[("_REENTRANT", None),
                                  ("PACKAGE", '"%s"' % "plutocracy"),
                                  ("PACKAGE_STRING", '"%s"' % PACKAGE_STRING),
                                  ("PKGDATADIR", '"%s"' %  PKGDATADIR)],
                   extra_link_args= ["-lSDL"],
                   depends = headers)

extensions.append( api_extension )

def df_walk(path):
    datafiles = []
    for x in os.walk(path):
        folder = x[0]
        files = x[2]
        files = [join(folder, x) for x in files ]
        folder = join("share", "plutocracy", folder)
        datafiles.append((folder, files))
    return datafiles

data_files  = []
data_files += df_walk("gui/")
data_files += df_walk("lang/")
data_files += df_walk("models/")
data_files += df_walk("configs/")

PackageData = {
               "name":        "plutocracy", 
               "version":     "0.0",
               "maintainer": "John Black",
               "maintainer_email": "nhojjohnnhoj@gmail.com",
               "packages":    ['plutocracy'],
               "package_dir": {'plutocracy': 'lib'},
               "data_files": data_files,
               "ext_modules": extensions
               }

# Pretty build output and skipping of unchanged files
ccompilier_class = distutils.ccompiler.new_compiler().__class__
old_compile = ccompilier_class._compile
old_link = ccompilier_class.link
def compile(self, obj, src, ext, cc_args, extra_postargs, pp_opts):
    mtime = os.path.getmtime(src)
    if d.has_key(src):
        if not headers_changed and d[src] == mtime:
            return
    d[src] = mtime
    distutils.log.info("\033[32mBuilding:\033[0m " + obj)
    distutils.log.set_verbosity(0)
    old_compile(self,  obj, src, ext, cc_args, extra_postargs, pp_opts)
    distutils.log.set_verbosity(1)
def link(self, target_desc, objects, output_filename, *args, **kwargs):
    distutils.log.info("\033[31mLinking:\033[0m " + output_filename)
    distutils.log.set_verbosity(0)
    old_link(self, target_desc, objects, output_filename, *args, **kwargs)
    distutils.log.set_verbosity(1)

ccompilier_class._compile = compile
ccompilier_class.link = link

# Gendoc
class Gendoc(Command):
    description = "Generate automatic documentation"
    user_options = []
    def initialize_options(self):
        pass
    def finalize_options(self):
        pass
    def gendoc_compile(self):
        compiler = ccompilier_class()
        compiler.output_dir = "build/gendoc/"
        sources = glob("gendoc/*.c")
        objects = compiler.compile(sources)
        compiler.link_executable(objects, "gendoc", output_dir="gendoc")
    def gendoc(self, output, in_path, title, inputs = []):
        output = os.path.join('gendoc', 'docs', output)
        inputs += glob(join(in_path, '*.h'))
        inputs += glob(join(in_path, '*.c'))
        gendoc_cmd = ["gendoc", "--file"]
        gendoc_header = ["--header", join("gendoc", "header.txt")]
        title_arg = ["--title", title]
        distutils.log.info("\033[34mGenerating Documentation for:\033[0m " +
                           title)
        distutils.log.set_verbosity(0)
        spawn(gendoc_cmd + [output] + title_arg + inputs)
        distutils.log.set_verbosity(1)

    def run(self):
        self.gendoc_compile()
        os.environ["PATH"] += ":" + join("gendoc", "")
        self.gendoc("gendoc.html", "gendoc", "GenDoc")
        self.gendoc('shared.html', join("src", "common"), "Plutocracy Shared",
                    [join("src", "render", "r_shared.h"),
                     join("src", "interface", "i_shared.h"),
                     join("src", "game", "g_shared.h"),
                     join("src", "network", "n_shared.h")] +
                     game_src + interface_src + render_src + network_src)
        self.gendoc('render.html', join("src", "render"), 'Plutocracy Render')
        self.gendoc('interface.html', join("src", "interface"),
                     'Plutocracy Interface')
        self.gendoc('game.html', join("src", "game"), 'Plutocracy Game')
        self.gendoc('network.html', join("src", "network"),
                     'Plutocracy Network')

setup(cmdclass={'gendoc': Gendoc}, **PackageData)
d.close() 