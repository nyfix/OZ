#!/bin/bash -x

[[ ${BUILD_TYPE} == "release"  ]] && PREFIX="taskset -c 1"

${PREFIX} `which mamaproducerc_v2` -tport ${MAMA_TPORT_PUB} -m ${MAMA_MW} \
-rt -steps 5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100 -stepInterval 10 \
$*
