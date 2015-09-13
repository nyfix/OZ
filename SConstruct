AddOption('--with-mamasource',
          default='/usr/local',
          dest='with_mamasource',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='location of the uncompiled OpenMAMA Source code')

AddOption('--with-mamainstall',
          default='/usr/local',
          dest='with_mamainstall',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='location of a compiled OpenMAMA installation prefix.')

SConscript('src/SConscript', variant_dir='objects', duplicate=0)
