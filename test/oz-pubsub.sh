#!/bin/bash -x

export WOMBAT_PATH=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)/../config

# select zmq transport, omnmnsg payload library
export MAMA_MW=zmq
export MAMA_PAYLOAD=omnmmsg
export MAMA_PAYLOAD_ID=O

# select transport from mama.properties
export MAMA_TPORT_PUB=pub
export MAMA_TPORT_SUB=sub

# set default log level
export MAMA_STDERR_LOGGING=4     # 2 logs only errors, higher values for more, lower for less

