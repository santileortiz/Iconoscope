#!/usr/bin/python3
from mkpy.utility import *
assert sys.version_info >= (3,2)

modes = {
        'debug': '-g -Wall',
        'profile_debug': '-O3 -g -pg -Wall',
        'release': '-O3 -DNDEBUG -DRELEASE_BUILD -Wall'
        }
cli_mode = get_cli_option('-M,--mode', modes.keys())
FLAGS = modes[pers('mode', 'debug', cli_mode)]
ensure_dir ("bin")

def default():
    target = pers ('last_target', 'iconoscope')
    call_user_function(target)

def iconoscope ():
    ex ('gcc {FLAGS} `pkg-config --cflags gtk+-3.0` -o bin/iconoscope iconoscope.c `pkg-config --libs gtk+-3.0` -lm')

if __name__ == "__main__":
    if get_cli_option ('--get_deps_pkgs'):
        get_target_dep_pkgs ()
        exit ()
    pymk_default()

