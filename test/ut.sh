#!/bin/bash -xv
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# run OpenMama unit tests
# (only avail if built from source)
`which UnitTestCommonC`          -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaC`            -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaPayloadC`     -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaMsgC`         -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
