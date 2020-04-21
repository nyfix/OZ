#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

export WOMBAT_PATH=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)/../config

# use omnmmsg payload if none specified
[[ -z ${MAMA_PAYLOAD} ]] && source ${SCRIPT_DIR}/omnmmsg.sh

# select zmq transport, omnmnsg payload library
export MAMA_MW=zmq

# select transport from mama.properties
export MAMA_TPORT_PUB=pub
export MAMA_TPORT_SUB=sub

# set default log level
#export MAMA_STDERR_LOGGING=4     # 2 logs only errors, higher values for more, lower for less

