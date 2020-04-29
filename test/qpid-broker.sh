#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# use qpidmsg payload if none specified
[[ -z ${MAMA_PAYLOAD} ]] && source ${SCRIPT_DIR}/qpidmsg.sh

# use mama.properties from OpenMAMA install
export WOMBAT_PATH=${INSTALL_BASE}/config

# select qpid transport w/broker
export MAMA_MW=qpid
export MAMA_TPORT_PUB=broker
export MAMA_TPORT_SUB=broker

