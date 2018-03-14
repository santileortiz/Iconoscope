#!/usr/bin/python3
from mkpy.utility import *
assert sys.version_info >= (3,2)

installation_info = {
    'bin/iconoscope': 'usr/bin/com.github.santileortiz.iconoscope',
    'data/iconoscope.desktop': 'usr/share/applications/com.github.santileortiz.iconoscope.desktop',
    'data/appdata.xml': 'usr/share/metainfo/com.github.santileortiz.iconoscope.appdata.xml',
    'data/128/iconoscope.svg': 'usr/share/icons/hicolor/128x128/apps/',
    'data/64/iconoscope.svg': 'usr/share/icons/hicolor/64x64/apps/',
    'data/48/iconoscope.svg': 'usr/share/icons/hicolor/48x48/apps/',
    'data/32/iconoscope.svg': 'usr/share/icons/hicolor/32x32/apps/',
    }

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

def install ():
    dest_dir = get_cli_option ('--destdir', has_argument=True)
    installed_files = install_files (installation_info, dest_dir)

    for f in installed_files:
        if 'hicolor' in f:
            ex ('gtk-update-icon-cache-3.0 /usr/share/icons/hicolor/')
            break;

if __name__ == "__main__":
    if get_cli_option ('--get_deps_pkgs'):
        get_target_dep_pkgs ()
        exit ()
    pymk_default()

