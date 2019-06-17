#!/bin/bash -xv

# run OpenMama unit tests
# (only avail if built from source)

`which UnitTestCommonC`          -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaC`            -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaPayloadC`     -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaMsgC`         -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
