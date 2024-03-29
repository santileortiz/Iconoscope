#!/usr/bin/python3
from mkpy import utility as cfg
from mkpy.utility import *
assert sys.version_info >= (3,2)

RDNN_NAME = 'com.github.santileortiz.iconoscope'
installation_info = {
    'bin/iconoscope': 'bin/{RDNN_NAME}',
    'data/iconoscope.desktop': 'share/applications/{RDNN_NAME}.desktop',
    'data/appdata.xml': 'share/metainfo/{RDNN_NAME}.appdata.xml',
    'data/128/iconoscope.svg': 'share/icons/hicolor/128x128/apps/{RDNN_NAME}.svg',
    'data/64/iconoscope.svg': 'share/icons/hicolor/64x64/apps/{RDNN_NAME}.svg',
    'data/48/iconoscope.svg': 'share/icons/hicolor/48x48/apps/{RDNN_NAME}.svg',
    'data/32/iconoscope.svg': 'share/icons/hicolor/32x32/apps/{RDNN_NAME}.svg',
    }

GTK_FLAGS = ex ('pkg-config --cflags --libs gtk+-3.0', ret_stdout=True, echo=False)

modes = {
        'debug': '-g -Wall',
        'profile_debug': '-O3 -g -pg -Wall',
        'release': '-O3 -DNDEBUG -DRELEASE_BUILD -Wall'
        }
mode = store('mode', get_cli_arg_opt('-M,--mode', modes.keys()), 'debug')
C_FLAGS = modes[mode]
ensure_dir ("bin")

def default():
    target = store_get ('last_snip', 'iconoscope')
    call_user_function(target)

def iconoscope ():
    ex ('gcc {C_FLAGS} -o bin/iconoscope iconoscope.c {GTK_FLAGS} -lm')

def install ():
    dest_dir = get_cli_arg_opt ('--destdir')
    print (f'Installing Iconoscope into: {dest_dir}')
    installed_files = install_files (installation_info, dest_dir)

    if dest_dir == None or dest_dir == '/':
        for f in installed_files:
            if 'hicolor' in f:
                ex ('gtk-update-icon-cache-3.0 /usr/share/icons/hicolor/')
                break;

def package_flatpak ():
    ex ("flatpak-builder --force-clean flatpak-build com.github.santileortiz.iconoscope.yml")

def package_deb ():
    if ex("dpkg-checkbuilddeps", echo=False) == 0 and ex ("debuild -i -us -uc -b") == 0:
        print(f"\nOutput files are located at: {ecma_bold(os.path.abspath('..'))}")
    else:
        print(ecma_red("\nerror:") + " Could not create deb package.")

cfg.builtin_completions = ['--get_run_deps', '--get_build_deps']
if __name__ == "__main__":
    # Everything above this line will be executed for each TAB press.
    # If --get_completions is set, handle_tab_complete() calls exit().
    handle_tab_complete ()

    pymk_default()

