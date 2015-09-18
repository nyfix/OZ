AddOption('--with-mamasource',
          default='/usr/local',
          dest='with_mamasource',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='Location of the uncompiled OpenMAMA Source code')

AddOption('--with-mamainstall',
          default='/usr/local',
          dest='with_mamainstall',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='Location of OpenMAMA install')

AddOption('--with-libevent',
          default='/usr/local',
          dest='with_libevent',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='Location of libevent install')

AddOption('--with-zmq',
          default='/usr/local',
          dest='with_zmq',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='Location of zeromq install')

AddOption('--target-arch',
          default=None,
          dest='target_arch',
          type='string',
          nargs=1,
          action='store',
          metavar='ARCH',
          help='Target architecture (x86 or x86_64)')

SConscript('src/SConscript', variant_dir='objects', duplicate=0)
