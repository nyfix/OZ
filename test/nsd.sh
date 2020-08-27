#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

[[ -z ${MAMA_NSD_PORT} ]] && echo "Must set MAMA_NSD_PORT!" && exit 1
[[ -z ${MAMA_NSD_ADDR} ]] && echo "Must set MAMA_NSD_ADDR!" && exit 1


# uncomment following to run w/process pinned to a CPU
#PREFIX="taskset -c 3"

${PREFIX} "${INSTALL_BASE}/bin/nsd" -i ${MAMA_NSD_ADDR} -p ${MAMA_NSD_PORT}