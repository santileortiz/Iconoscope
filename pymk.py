#!/usr/bin/python3
from mkpy.utility import *
assert sys.version_info >= (3,2)

RDNN_NAME = 'com.github.santileortiz.iconoscope'
installation_info = {
    'bin/iconoscope': 'usr/bin/{RDNN_NAME}',
    'data/iconoscope.desktop': 'usr/share/applications/{RDNN_NAME}.desktop',
    'data/appdata.xml': 'usr/share/metainfo/{RDNN_NAME}.appdata.xml',
    'data/128/iconoscope.svg': 'usr/share/icons/hicolor/128x128/apps/{RDNN_NAME}.svg',
    'data/64/iconoscope.svg': 'usr/share/icons/hicolor/64x64/apps/{RDNN_NAME}.svg',
    'data/48/iconoscope.svg': 'usr/share/icons/hicolor/48x48/apps/{RDNN_NAME}.svg',
    'data/32/iconoscope.svg': 'usr/share/icons/hicolor/32x32/apps/{RDNN_NAME}.svg',
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

    if dest_dir == None or dest_dir == '/':
        for f in installed_files:
            if 'hicolor' in f:
                ex ('gtk-update-icon-cache-3.0 /usr/share/icons/hicolor/')
                break;

if __name__ == "__main__":
    if get_cli_option ('--get_deps_pkgs'):
        get_target_dep_pkgs ()
        exit ()
    pymk_default()

