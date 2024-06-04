#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# use omnmmsg payload if none specified
[[ -z ${MAMA_PAYLOAD} ]] && source ${SCRIPT_DIR}/omnmmsg.sh

# use mama.properties from here
[[ -z ${WOMBAT_PATH} ]] && export WOMBAT_PATH=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)/../config

# use zmq transport w/nsd
export MAMA_MW=zmq
export MAMA_TPORT_PUB=oz
export MAMA_TPORT_SUB=oz
[[ -z ${MAMA_NSD_ADDR} ]] && export MAMA_NSD_ADDR=127.0.0.1
[[ -z ${MAMA_NSD_PORT} ]] && export MAMA_NSD_PORT=5756

# set default log level
#export MAMA_STDERR_LOGGING=4     # 2 logs only errors, higher values for more, lower for less

