#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

[[ -z ${MAMA_TPORT_PUB} ]] && echo "Must set MAMA_TPORT_PUB!" && exit 1
[[ -z ${MAMA_MW} ]] && echo "Must set MAMA_MW!" && exit 1

# uncomment following to run w/process pinned to a CPU
#PREFIX="taskset -c 1"

${PREFIX} `which mamaproducerc_v2` -tport ${MAMA_TPORT_PUB} -m ${MAMA_MW} $*
