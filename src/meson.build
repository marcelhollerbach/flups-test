e = dependency('enlightenment')
x11 = dependency('x11')
xi = dependency('xi')

module = shared_module('module',
                       'e_mod_main.c',
                       dependencies : e,
                       install_dir: install_dir,
                       install: true,
                       name_prefix: '',
                       link_args: '-Wl,--unresolved-symbols=ignore-in-object-files')

keylogger = executable('keylogger', 'keylogger.c',
                       dependencies : [x11,xi],
                       install_dir: install_dir,
                       install: true)
