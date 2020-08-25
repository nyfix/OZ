set auto-load safe-path /
set breakpoint pending on
set height 0


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

b mamaSubscription_createBasicWildCard
b zmqBridgeMamaSubscription_createWildCard
b oz::subscriber::start
b oz::subscriber::is_valid_regex_string
