set auto-load safe-path /
set breakpoint pending on
set height 0


# find the printers.py file associated with current compiler
python
# (typically in usr/share/<compiler-version>/python/libstdcxx/v6/printers.py), installed from
cmd = "echo -n $(dirname $(find $(cd $(dirname $(which gcc))/.. && /bin/pwd) -name printers.py 2>/dev/null))"
import os
tmp = os.popen(cmd).read()

# import the pretty printers
import sys
sys.path.insert(0, tmp)
from printers import register_libstdcxx_printers
register_libstdcxx_printers (None)
end


# add source directories
python
import os
gdb.execute('directory' + os.environ['BUILD_ROOT'] + '/libzmq/src')
gdb.execute('directory' + os.environ['BUILD_ROOT'] + '/OpenMAMA')
gdb.execute('directory' + os.environ['BUILD_ROOT'] + '/OpenMAMA-omnm/src')
gdb.execute('directory' + '../src')
end


handle SIGINT stop pass

# non-dispatching queue
#b queue.c:295
