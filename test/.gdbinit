set breakpoint pending on
set height 0

python
import os
gdb.execute('directory' + os.environ['OPENMAMA_ROOT'] + '/src')
gdb.execute('directory' + os.environ['ZMQ_ROOT'] + '/src')
gdb.execute('directory' + os.environ['OPENMAMA_ZMQ_ROOT'] + '/src')
gdb.execute('directory' + os.environ['OPENMAMA_OMNM_ROOT'] + '/src')
end

set args  -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
