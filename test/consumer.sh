#!/bin/bash -xv
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

#[[ ${BUILD_TYPE} == "release"  ]] && PREFIX="taskset -c 2"

${PREFIX} `which mamaconsumerc_v2` -tport ${MAMA_TPORT_SUB} -m ${MAMA_MW} \
-percentiles 90,99,99.9,99.99 \
$*
