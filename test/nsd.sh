#!/bin/bash -x

[[ ${BUILD_TYPE} == "release"  ]] && PREFIX="taskset -c 3"

${PREFIX} nsd -i ${MAMA_NSD_ADDR} -p ${MAMA_NSD_PORT}