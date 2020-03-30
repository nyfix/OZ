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

set args  -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}

