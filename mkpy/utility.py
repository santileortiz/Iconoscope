import sys, subprocess, os, ast, shutil

import importlib.util, inspect, pathlib, filecmp

"""
!!!!!!IMPOTRANT!!!!!
The idea of this library is to make easy to call python functions located in
the file pymk.py through the command line. Every function created in that file
is called a build target because we provide some extra functionality for them.
At the moment some of these functionalities are:

    - Automatic TAB completions.
    - Automatic guessing of packages that provide libraries used as gcc flags.
    - Calling './pymk.py my_target' calls my_target

To be able to provide these we need to call the targets in a dry_run mode that
won't have side effects besides updating several global variables in this
module. For this reason when editing/creating a target function the user has to
have in mind targets may be called at unexpected times but ONLY in dry_run
mode. If the target calls functions with side effects besides the ones in this
module then we tell the user we are in dry_mode run through the g_dry_run
global variable. Which means the user should separate code with side effects as
follows:

    def my_target():
        global g_dry_run
        if not g_dry_run:
            <code with side effects>

"""

def get_functions():
    """
    Returns a list of functions defined in the __main__ module, it's a list of
    tuples like [(f_name, f), ...].

    NOTE: Functions imported like 'from <module> import <function>' are
    included too.
    """
    return inspect.getmembers(sys.modules['__main__'], inspect.isfunction).copy()

def get_user_functions():
    """
    Like get_functions() removing the ones defined in this file.
    """
    keys = globals().copy().keys()
    return [(m,v) for m,v in get_functions() if m not in keys]

def call_user_function(name, dry_run=False):
    fun = None
    for f_name, f in get_user_functions():
        if f_name == name:
            fun = f
            break

    global g_dry_run
    if dry_run:
        g_dry_run = True

    fun() if fun else print ('No function: '+name)

    if dry_run:
        g_dry_run = False
    return

def check_completions ():
    comp_path = pathlib.Path('/usr/share/bash-completion/completions/pymk.py')
    if not comp_path.exists():
        warn ('Tab completions not installed:')
        print ('Use "sudo ./pymk.py --install_completions" to install them\n')
        return
    if comp_path.is_file() and not filecmp.cmp ('mkpy/pymk.py', str(comp_path)):
        err ('Tab completions outdated:')
        print ('Update with "sudo ./pymk.py --install_completions"\n')

def default_opt (s):
    opt_lst = s.split(',')
    res = None
    for s in opt_lst: 
        if s.startswith('--'):
            res = s
    if res == None:
        res = opt_lst[0]
    return res

cli_completions = {}
def cli_completion_options ():
    """
    Handles command line options used by tab completion.
    The option --get_completions exits the script after printing completions.
    """
    global cli_completions

    if get_cli_option('--install_completions'):
        ex ("cp mkpy/pymk.py /usr/share/bash-completion/completions/")

    data_str = get_cli_option('--get_completions', unique_option=True, has_argument=True)
    if data_str != None:
        data_lst = data_str.split(' ')
        curs_pos = int(data_lst[0])
        line = data_lst[1:]
        if len(line) == 1: line.append('')

        match_found = False
        for opt,vals in cli_completions.items():
            if line[-2] in opt.split(',') and vals != None:
                match_found = True
                print (' '.join(vals))

        if not match_found:
            f_names = [s for s,f in get_user_functions()]
            print (' '.join(f_names))
            if line[-1] == '-':
                def_opts = [default_opt(s) for s in cli_completions.keys()]
                print (' '.join(def_opts))
            elif line[-1] != '':
                def_opts = []
                for s in cli_completions.keys(): def_opts += s.split(',')
                print (' '.join(def_opts))
        exit ()

def get_cli_option (opts, values=None, has_argument=False, unique_option=False):
    """
    Parses sys.argv looking for option _opt_.

    If _opt_ is not found, returns None.
    If _opt_ does not have arguments, returns True if the option is found.
    If _opt_ has an argument, returns the value of the argument. In this case
    additional error checking is available if _values_ is set to a list of the
    possible values the argument could take.

    NOTE: We don't detect if there is an argument, the caller must tell if it
    expects an argument or not by using has_argument.

    When unique_option is True then _opt_ must be the only option used.

    """
    global cli_completions

    res = None
    i = 1
    if values != None: has_argument = True
    cli_completions[opts] = values

    while i<len(sys.argv):
        if sys.argv[i] in opts.split(','):
            if has_argument:
                if i+1 >= len(sys.argv):
                    print ('Missing argument for option '+opt+'.');
                    if values != None:
                        print ('Possible values are [ '+' | '.join(values)+' ].')
                    return
                else:
                    res = sys.argv[i+1]
                    break
            else:
                res = True
        i = i+1

    if unique_option and res != None:
        if (has_argument and len(sys.argv) != 3) or  (not has_argument and len(sys.argv) != 2):
            print ('Option '+opt+' receives no other option.')
            return

    if values != None and res != None:
        if res not in values:
            print ('Argument '+res+' is not valid for option '+opt+',')
            print ('Possible values are: [ '+' | '.join(values)+' ].')
            return
    return res

def get_cli_rest ():
    """
    Returns an array of all argv values that are after options starting with '-'.
    """
    i = 1;
    while i<len(sys.argv):
        if sys.argv[i].startswith ('-'):
            if len(sys.argv) > i+1 and not sys.argv[i+1].startswith('-'):
                i = i+1
        else:
            return sys.argv[i:]
        i = i+1
    return None

def err (string):
    print ('\033[1m\033[91m{}\033[0m'.format(string))

def ok (string):
    print ('\033[1m\033[92m{}\033[0m'.format(string))

def get_user_str_vars ():
    """
    Returns a dictionary with global strings in module __main__.
    """
    var_list = inspect.getmembers(sys.modules['__main__'])
    var_dict = {}
    for v_name, v in var_list:
        if type(v) == type(""):
            var_dict[v_name] = v
    return var_dict

def get_deps_pkgs (flags):
    """
    Prints dpkg packages from where -l* options in _flags_ are comming from.
    """
    # TODO: Is there a better way to find out this information?
    libs = [f for f in flags.split(" ") if f.startswith("-l")]
    strs = ex ('ld --verbose '+" ".join(libs)+' | grep succeeded | grep -Po "/\K(/.*.so)" | xargs dpkg-query --search', ret_stdout=True, echo=False)
    ex ('rm a.out', echo=False) # ld always creates a.out
    res = ex ('echo "' +str(strs)+ '" | grep -Po "^.*?(?=:)" | sort | uniq | xargs echo', ret_stdout=True, echo=False)
    print (res[:-1])

ex_cmds = []
g_dry_run = False

def set_dry_run():
    global g_dry_run
    g_dry_run = True

def ex (cmd, no_stdout=False, ret_stdout=False, echo=True):
    global g_dry_run

    resolved_cmd = cmd.format(**get_user_str_vars())

    if g_dry_run:
        ex_cmds.append(resolved_cmd)
        return

    if echo: print (resolved_cmd)

    if not ret_stdout:
        redirect = open(os.devnull, 'wb') if no_stdout else None
        return subprocess.call(resolved_cmd, shell=True, stdout=redirect)
    else:
        return subprocess.check_output(resolved_cmd, shell=True, stderr=open(os.devnull, 'wb')).decode()

def get_target ():
    if len(sys.argv) == 1:
        return 'default'
    else:
        if sys.argv[1].startswith ('-'):
            return 'default'
        else:
            return sys.argv[1]

def get_target_dep_pkgs ():
    global ex_cmds
    call_user_function (get_target(), dry_run=True)
    for s in ex_cmds:
        if s.startswith('gcc'):
            get_deps_pkgs (s)

def pers (name, default=None, value=None):
    """
    Makes persistent some value across runs of the script storing it in a
    dctionary on "mkpy/cache".  Stores _name_:_value_ pair in cache unless
    value==None.  Returns the value of _name_ in the cache.

    If default is used, when _value_==None and _name_ is not in the cache the
    pair _name_:_default is stored.
    """
    global g_dry_run
    if g_dry_run:
        return

    cache_dict = {}
    if os.path.exists('mkpy/cache'):
        cache = open ('mkpy/cache', 'r')
        cache_dict = ast.literal_eval (cache.readline())
        cache.close ()

    if value == None:
        if name in cache_dict.keys ():
            return cache_dict[name]
        else:
            if default != None:
                cache_dict[name] = default
            else:
                print ('Key '+name+' is not in cache.')
                return
    else:
        cache_dict[name] = value

    cache = open ('mkpy/cache', 'w')
    cache.write (str(cache_dict)+'\n')
    return cache_dict.get (name)

# This could also be accomplished by:
#   ex ('mkdir -p {path_s}')
# Maybe remove this function and use that instead, although it won't work on Windows
def ensure_dir (path_s):
    global g_dry_run
    if g_dry_run:
        return

    resolved_path = path_s.format(**get_user_str_vars())

    path = pathlib.Path(resolved_path)
    if not path.exists():
        os.makedirs (resolved_path)

def install_files (info_dict, prefix=None):
    global g_dry_run

    if prefix == None:
        prefix = '/'

    prnt = [] 
    for f in info_dict.keys():
        dst = prefix + info_dict[f]
        resolved_dst = dst.format(**get_user_str_vars())
        dst_path = pathlib.Path(resolved_dst)

        resolved_f = f.format(**get_user_str_vars())

        dst_dir, fname = os.path.split (resolved_dst)
        if fname == '':
            _, fname = os.path.split (resolved_f)

        dest_file = dst_dir + '/' + fname
        if not g_dry_run:
            if not dst_path.exists():
                os.makedirs (resolved_dst)
            shutil.copy (resolved_f, dest_file)
            prnt.append (dest_file)
        else:
            prnt.append ('Install: ' + resolved_f + ' -> ' + dest_file)

    prnt.sort()
    [print (s) for s in prnt]

def pymk_default ():
    check_completions ()
    cli_completion_options() # If we are being called by the tab completion script, execution ends here

    t = get_target()
    call_user_function (t)
    if t != 'default' and t != 'install':
        pers ('last_target', value=t)

