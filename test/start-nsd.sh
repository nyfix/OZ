#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# start nsd if necessary
if [[ ${MAMA_TPORT_PUB} = "oz" ]]; then
   pidof "${INSTALL_BASE}/bin/nsd"
   if [[ $? -ne 0 ]]; then
      "${SCRIPT_DIR}/nsd.sh" &
      sleep 2
      exit 1
   fi
fi
