#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# uncomment following to run w/process pinned to a CPU
#PREFIX="taskset -c 3"

${PREFIX} nsd -i ${MAMA_NSD_ADDR} -p ${MAMA_NSD_PORT}