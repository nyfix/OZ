#!/bin/bash -x

# OpenMAMA
export OPENMAMA_ROOT=${INSTALL_BASE}/OpenMAMA/${OPENMAMA_VERSION}/${BUILD_TYPE}
export PATH=${OPENMAMA_ROOT}/bin:$PATH
export LD_LIBRARY_PATH=${OPENMAMA_ROOT}/lib:$LD_LIBRARY_PATH

# ZeroMQ
export ZMQ_ROOT=${INSTALL_BASE}/libzmq/${LIBZMQ_VERSION}/${BUILD_TYPE}
export PATH=${ZMQ_ROOT}/bin:$PATH
export LD_LIBRARY_PATH=${ZMQ_ROOT}/lib64:$LD_LIBRARY_PATH

# OpenMAMA-omnm
export OPENMAMA_OMNM_ROOT=${INSTALL_BASE}/OpenMAMA-omnm/${OPENMAMA_OMNM_VERSION}/${BUILD_TYPE}
export LD_LIBRARY_PATH=${OPENMAMA_OMNM_ROOT}/lib:$LD_LIBRARY_PATH

# OpenMAMA-zmq
export OPENMAMA_ZMQ_ROOT=${INSTALL_BASE}/OpenMAMA-zmq/${PROJECT_VERSION}/${BUILD_TYPE}
export PATH=${OPENMAMA_ZMQ_ROOT}/bin:$PATH
export LD_LIBRARY_PATH=${OPENMAMA_ZMQ_ROOT}/lib:$LD_LIBRARY_PATH

# mama.properties
export WOMBAT_PATH=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)

# use loopback interface
export MAMA_NSD_ADDR=127.0.0.1
export MAMA_PUBLISH_ADDR=lo
export MAMA_NSD_PORT=5756

# select zmq transport, omnmnsg payload library
export MAMA_MW=zmq
export MAMA_PAYLOAD=omnmmsg
export MAMA_PAYLOAD_ID=O
# select transport from mama.properties
export MAMA_TPORT_PUB=oz
export MAMA_TPORT_SUB=oz

# set default log level
export MAMA_STDERR_LOGGING=2     # 2 logs only errors, higher values for more, lower for less
