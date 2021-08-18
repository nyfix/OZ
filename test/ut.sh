#!/bin/bash -xv
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# set middleware, payload if not set
[[ -z ${MAMA_MW} ]] && source ${SCRIPT_DIR}/oz-nsd.sh
[[ -z ${MAMA_PAYLOAD} ]] && source ${SCRIPT_DIR}/omnmmsg.sh

# start nsd if necessary
${SCRIPT_DIR}/start-nsd.sh

set -eo pipefail

# run OpenMama unit tests
UnitTestCommonC          -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
UnitTestMamaC            -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
UnitTestMamaPayloadC     -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
UnitTestMamaMsgC         -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}

${SCRIPT_DIR}/stop-nsd.sh
