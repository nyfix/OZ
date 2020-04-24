#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# uncomment following to run w/process pinned to a CPU
#PREFIX="taskset -c 1"

${PREFIX} `which mamaproducerc_v2` -tport ${MAMA_TPORT_PUB} -m ${MAMA_MW} $*
