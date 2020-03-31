#!/bin/bash -xv
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# set middleware, payload if not set
[[ -z ${MAMA_MW} ]] && source ${SCRIPT_DIR}/oz-nsd.sh
[[ -z ${MAMA_PAYLOAD} ]] && source ${SCRIPT_DIR}/omnmmsg.sh

# start nsd if necessary
if [[ ${MAMA_TPORT_PUB} = "nsd" ]]; then
   pidof "${INSTALL_BASE}/bin/nsd"
   if [[ $? -ne 0 ]]; then
      "${SCRIPT_DIR}/nsd.sh" &
      sleep 2
      KILLNSD=1
   fi
fi

# run OpenMama unit tests
`which UnitTestCommonC`          -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaC`            -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaPayloadC`     -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}
`which UnitTestMamaMsgC`         -m ${MAMA_MW} -p ${MAMA_PAYLOAD} -i ${MAMA_PAYLOAD_ID}

[[ ${KILLNSD} -eq 1 ]] && killall "${INSTALL_BASE}/bin/nsd"
