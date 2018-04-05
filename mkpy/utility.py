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
        print ('Tab completions not installed:')
        print ('Use "sudo ./pymk.py --install_completions" to install them\n')
        return False
    else:
        return True

# Options can have several syntaxes, like ls has -a and --all. These are stored
# as a comma separated list in the key of cli_completions, here we choose just
# one to recommend when using tab completions.
def recommended_opt (s):
    opt_lst = s.split(',')
    res = None
    for s in opt_lst: 
        if s.startswith('--'):
            res = s
    if res == None:
        res = opt_lst[0]
    return res

builtin_completions = []
cli_completions = {}
def handle_tab_complete ():
    """
    Handles command line options used by tab completion.
    The option --get_completions exits the script after printing completions.
    """
    global cli_completions, builtin_completions

    # Check that the tab completion script is installed
    if not check_completions ():
        return

    # Add the builtin tab completions the user wants
    if len(builtin_completions) > 0:
        [get_cli_option (c) for c in builtin_completions]

    if get_cli_option('--install_completions'):
        ex ("cp mkpy/pymk.py /usr/share/bash-completion/completions/")
        exit ()

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
                def_opts = [recommended_opt(s) for s in cli_completions.keys()]
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

def pkg_config_libs (packages):
    return ex ('pkg-config --libs ' + ' '.join(packages), ret_stdout=True)

def pkg_config_includes (packages):
    return ex ('pkg-config --cflags ' + ' '.join(packages), ret_stdout=True)

ex_cmds = []
g_dry_run = False

def set_dry_run():
    global g_dry_run
    g_dry_run = True

def ex_escape (s):
    return s.replace ('\n', '').replace ('{', '{{').replace ('}','}}')

def ex (cmd, no_stdout=False, ret_stdout=False, echo=True):
    # NOTE: This fails if there are braces {} in cmd but the content is not a
    # variable. If this is the case, escape the content that has braces using
    # the ex_escape() function. This is required for things like awk scripts.
    global g_dry_run

    resolved_cmd = cmd.format(**get_user_str_vars())

    ex_cmds.append(resolved_cmd)
    if g_dry_run:
        return

    if echo: print (resolved_cmd)

    if not ret_stdout:
        redirect = open(os.devnull, 'wb') if no_stdout else None
        return subprocess.call(resolved_cmd, shell=True, stdout=redirect)
    else:
        return subprocess.check_output(resolved_cmd, shell=True, stderr=open(os.devnull, 'wb')).decode().strip ()

def info (s):
    # The following code can be used to se available colors
    #for i in range (8):
    #    print ("\033[0;3" + str(i) + "m\033[K" + "HELLO" + "\033[m\033[K", end=' ')
    #    print ("\033[0;9" + str(i) + "m\033[K" + "HELLO" + "\033[m\033[K", end=' ')
    #    print ("\033[1;9" + str(i) + "m\033[K" + "HELLO" + "\033[m\033[K", end=' ')
    #    print ("\033[1;3" + str(i) + "m\033[K" + "HELLO" + "\033[m\033[K")

    #for i in range (16, 232):
    #    print (str(i) + ": \033[1;38;5;" + str(i) + "m\033[K" + "HELLO" + "\033[m\033[K")

    color = '\033[1;38;5;177m\033[K'
    default_color = '\033[m\033[K'
    print (color+s+default_color)

def get_target ():
    if len(sys.argv) == 1:
        return 'default'
    else:
        if sys.argv[1].startswith ('-'):
            return 'default'
        else:
            return sys.argv[1]

def pers_get_cache_dict ():
    cache_dict = {}
    if os.path.exists('mkpy/cache'):
        cache = open ('mkpy/cache', 'r')
        cache_dict = ast.literal_eval (cache.readline())
        cache.close ()
    return cache_dict

def pers_set_cache_dict (cache_dict):
    cache = open ('mkpy/cache', 'w')
    cache.write (str(cache_dict)+'\n')
    cache.close ()

def pers (name, default=None, value=None):
    """
    Makes persistent some value across runs of the script storing it in a
    dctionary on "mkpy/cache".  Stores _name_:_value_ pair in cache unless
    value==None.  Returns the value of _name_ in the cache.

    If default is used, when _value_==None and _name_ is not in the cache the
    pair _name_:_default is stored.
    """
    global g_dry_run

    cache_dict = pers_get_cache_dict ()

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

    # In dry ryn mode just don't update the cache file
    if not g_dry_run:
        pers_set_cache_dict (cache_dict)
    return cache_dict.get (name)

def pers_func_f (name, func, args, kwargs={}):
    # TODO: Is there a valid usecase where arg==None? for now I can't think of
    # any one.
    if args == None:
        print ("I didn't expect to receive arg==None")
        return

    call_func = False
    cache_dict = pers_get_cache_dict ()

    # If args changed from last call update them in cache and trigger
    # execution of func
    last_args = None
    if name+'_args' in cache_dict.keys ():
        last_args = cache_dict[name+'_args']

    if last_args != args:
        cache_dict[name+'_args'] = args
        call_func = True

    # If kwargs changed from last call update them in cache and trigger
    # execution of func
    last_kwargs = {}
    if kwargs != {}:
        if name+'_kwargs' in cache_dict.keys ():
            last_kwargs = cache_dict[name+'_kwargs']

        if last_kwargs != kwargs:
            cache_dict[name+'_kwargs'] = kwargs
            call_func = True

    # If some argument changed or func has never been called, call func and
    # store result in cache. Else return cached value.
    if last_args == None or call_func:
        res = func(*args, **kwargs)
        cache_dict[name] = res
        pers_set_cache_dict (cache_dict)
        return res
    else:
        return cache_dict[name]

def pers_func (name, func, arg):
    """
    Simpler version of pers_func_f for the case when _func_ has a single
    argument
    """
    return pers_func_f (name, func, [arg])

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

def file_time(fname):
    res = 0
    tgt_path = pathlib.Path(fname)
    if tgt_path.is_file():
        res = os.stat(fname).st_mtime
    return res

def install_files (info_dict, prefix=None):
    global g_dry_run

    if prefix == None:
        prefix = '/'

    prnt = []
    for f in info_dict.keys():
        dst = prefix + info_dict[f]
        resolved_dst = dst.format(**get_user_str_vars())
        resolved_f = f.format(**get_user_str_vars())

        # Compute the absolute dest path including the file, even if just a
        # dest directory was specified
        dst_dir, fname = os.path.split (resolved_dst)
        if fname == '':
            _, fname = os.path.split (resolved_f)
        dest_file = dst_dir + '/' + fname

        # If the file already exists check we have a newer version. If we
        # don't, skip it.
        install_file = True
        dest_file_path = pathlib.Path(dest_file)
        if dest_file_path.exists():
            src_time = file_time (resolved_f)
            dst_time = file_time (dest_file)
            if (src_time < dst_time):
                install_file = False

        if install_file:
            if not g_dry_run:
                dst_path = pathlib.Path(dst_dir)
                if not dst_path.exists():
                    os.makedirs (dst_dir)

                shutil.copy (resolved_f, dest_file)
                prnt.append (dest_file)
            else:
                prnt.append ('Install: ' + resolved_f + ' -> ' + dest_file)

    prnt.sort()
    [print (s) for s in prnt]

    return prnt

# TODO: dnf is written in python, meybe we can call dnf's module instead of
# using ex().
# @use_dnf_python
def rpm_find_providers (file_list):
    f_str = " ".join (file_list)
    cmd = ex_escape (r"rpm -qf " + f_str + " --queryformat '%{NAME}\n' | sort | uniq")
    prov_str = ex (cmd, ret_stdout=True, echo=False)
    return prov_str.split ('\n')

# @use_dnf_python
def rpm_find_deps (pkg_name):
    # FIXME: The --installed option is required because there is a bug in dnf
    # that makes the query impossibly slow. Check if this gets fixed.
    cmd = ex_escape(r"dnf repoquery --qf '%{NAME}' --requires --resolve --installed --recursive " + pkg_name)
    deps_str = ex (cmd, ret_stdout=True, echo=False)
    return deps_str.split ('\n')

def deb_find_providers (file_list):
    f_str = " ".join (file_list)
    cmd = ex_escape ("dpkg -S " + f_str + " | awk -F: '{print $1}' | sort | uniq")
    prov_str = ex (cmd, ret_stdout=True, echo=False)
    return prov_str.split ('\n')

def deb_find_deps (pkg_name):
    flags = '--recurse --no-recommends --no-suggests --no-conflicts --no-breaks --no-replaces --no-enhances'
    apt_cmd = 'apt-cache depends {} {}'.format(flags, pkg_name)
    cmd = ex_escape (apt_cmd + " | grep Depends | awk '{print $2}'")
    deps_str = ex (cmd, ret_stdout=True, echo=False)
    return deps_str.split ('\n')

def get_pkg_manager_type ():
    os_release = open ('/etc/os-release', "r")
    os_id = ''
    os_id_like = ''
    for l in os_release:
        if l.startswith ('ID='):
            os_id = l.replace ('ID=', '').strip()
        elif l.startswith ('ID_LIKE='):
            os_id_like = l.replace ('ID_LIKE=', '').strip()
    os_release.close()

    os_id = os_id.replace ("'",'').replace ('"', '')
    os_id_like = os_id_like.replace ("'",'').replace ('"', '')

    def match_os_id (wanted_ids, os_id, os_id_like):
        for i in wanted_ids:
            if i == os_id:
                return True
            elif i == os_id_like:
                return True
            else:
                return False

    deb_oses = ['elementary', 'ubuntu', 'debian']
    rpm_oses = ['fedora']

    if match_os_id (deb_oses, os_id, os_id_like):
        return 'deb'
    elif match_os_id (rpm_oses, os_id, os_id_like):
        return 'rpm'
    else:
        return ''

pkg_manager_type = get_pkg_manager_type ()

# Probably put this in polymorphic classes?
# TODO: Test this in several distributions. Arch is still not implemented.
find_deps = None
find_providers = None
if pkg_manager_type == 'deb':
    find_deps = deb_find_deps
    find_providers = deb_find_providers
elif pkg_manager_type == 'rpm':
    find_deps = rpm_find_deps
    find_providers = rpm_find_providers

def prune_pkg_list (pkg_list):
    a = set()
    b = set(pkg_list)

    while b:
        dep = b.pop ()
        curr_deps = find_deps (dep)
        #print ('Pruning {}: {}\n'.format(dep, " ".join(curr_deps)))
        print ('Pruning {}... '.format(dep), end='', flush=True)
        removed = []
        for d in curr_deps:
            if d in a:
                removed.append(d)
                a.remove (d)
            elif d in b:
                removed.append(d)
                b.remove (d)
        a.add (dep)

        if len(removed) > 1:
            print ('Removes: ' + ' '.join(removed))
        else:
            print ()
    print ()
    return list (a)

def gcc_used_system_includes (cmd):
    tmpfname = ex ('mktemp', ret_stdout=True, echo=False)
    arr = cmd.split()
    arr.insert (1, '-M -MF {}'.format(tmpfname))

    # FIXME: If we call gcc -M -MF <file> -o <output_file> then gcc for some
    # reason clears <output_file> and becomes empty. Strip the -o option to
    # avoid this.
    for i, tk in enumerate(arr):
        if tk == '-o':
            # Remove -o
            arr.pop(i)
            # Remove <output_file>
            arr.pop(i)
            break

    ex (" ".join (arr), echo=False)

    # NOTE: I think gcc outputs one Makefile rule per source file present in
    # the command, right now we just take all system include files and merge
    # them.
    awk_prg = r"""
    BEGIN {
        RS="\\";
        OFS="\n"
    }
    {
        for(i=1;i<=NF;i++)
            if ($i ~ /^\//) {print $i}
    }
    """
    awk_prg = ex_escape(awk_prg)
    res = ex ("awk '{}' {} | sort | uniq".format(awk_prg, tmpfname), echo=False, ret_stdout=True)
    return res.split ('\n')

def gcc_get_out (cmd):
    """
    Returns a.out or the file after -o option.
    """
    out_f = 'a.out'
    cmd_arr = cmd.split ()
    for i, tk in enumerate(cmd_arr):
        if tk == '-o':
            out_f = cmd_arr[i+1]
            break
    return out_f

def file_is_elf (fname):
    return not ex ('file '+ fname + ' | grep ELF', echo=False, no_stdout=True)

def file_exists (fname):
    return pathlib.Path(fname).exists()

def pymk_default ():
    global ex_cmds
    t = get_target()

    if '--get_run_deps' in builtin_completions and get_cli_option ('--get_run_deps'):
        # Look for all gcc commands that generate an executable and get the
        # packages that provide the shared libraries it uses.
        #
        # Note that shared libraries is not the only way a project can have a
        # runtime dependency. These are some ways an executable may have a
        # build dependency that won't be discovered by this code:
        #   * Shared libraries that are conditionally loaded by the application
        #     using dlopen().
        #   * Any file opened by the application that is provided by other
        #     project. For example configuration files like /etc/os-release
        #     which is provided by systemd, or icon themes.
        #
        # First we try to get the executable using a dry run of the target, if
        # we can't find the output file then runs the target and try again. I'm
        # still not sure this is a good default because if a target generates
        # something using gcc and then deletes it, then this function will
        # always call the target. Still, knowing if a target deletes gcc's
        # output seems intractable, there are too many ways to do it.
        if pkg_manager_type == '':
            exit ()

        use_dry_run = True
        for attempt in range (2):
            call_user_function (t, dry_run=use_dry_run)
            tmp_ex_cmds = ex_cmds[:]
            for cmd in tmp_ex_cmds:
                if cmd.startswith('gcc'):
                    out_file = gcc_get_out (cmd)
                    if not file_exists (out_file):
                        if attempt == 0:
                            # Output does not exist, reset ex_cmd and try
                            # again but not as dry_run.
                            ex_cmds = []
                            use_dry_run = False
                            break
                        else:
                            # Output does not exist but we already ran the
                            # target. Probably the target itself deletes this
                            # file. Skip it.
                            continue
                    elif file_is_elf (out_file):
                        info ('Runtime dependencies for: ' + out_file)
                        tmp = ex_escape("ldd " + out_file + " | awk '!/vdso/{print $(NF-1)}'")
                        required_shdeps = ex (tmp, ret_stdout=True, echo=False).split ('\n')
                        deps = find_providers (required_shdeps)
                        print (' '.join (deps))
                        print ()

                        deps = prune_pkg_list (deps)
                        deps.sort()
                        info ('Minimal runtime dependencies for: ' + out_file)
                        print (' '.join (deps))
                        print ()
            if use_dry_run == True:
                # Calling the target wasn't necessary, we are done.
                break
        exit ()

    if '--get_build_deps' in builtin_completions and get_cli_option ('--get_build_deps'):
        # Call the target in dry run mode and for each gcc command find the
        # packages that provide the include files it needs.
        #
        # Note that included files are not the only way a project gets build
        # dependencies. For instance using this script adds a dependency on
        # python3 (this one we add by default). Other ways in which you may
        # have build dependencies that won't be discovered by this code are:
        #
        #   * Build toolchain (gcc, llvm, ld, etc.).
        #   * Any non default python module used in pymk.py.
        #   * Any non standard command caled by the ex() function.
        #   * Tools used to package the application (rpmbuild, debuild).
        #   * Dependencies of statically linked libraries.

        if pkg_manager_type == '':
            exit ()

        call_user_function (t, dry_run=True)
        deps = set()
        for cmd in ex_cmds[:]:
            if cmd.startswith('gcc'):
                include_files = gcc_used_system_includes (cmd)
                curr_deps = set (find_providers (include_files))
                deps = deps.union (curr_deps)

        # Sort and print
        deps_list = list(deps)
        deps_list.append ('python3')
        deps_list.sort()
        info ('Build dependencies for target: ' + t)
        print (' '.join(deps_list), end='\n\n')

        deps_list = prune_pkg_list (deps_list)
        deps_list.sort()
        info ('Minimal build dependencies for target: ' + t)
        print (" ".join (deps_list), end='\n\n')
        exit ()

    call_user_function (t)
    if t != 'default' and t != 'install':
        pers ('last_target', value=t)

