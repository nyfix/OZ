#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh
source ${SCRIPT_DIR}/oz-nsd.sh

# start nsd if necessary
pidof "${INSTALL_BASE}/bin/nsd"
if [[ $? -ne 0 ]]; then
   nohup "${SCRIPT_DIR}/nsd.sh" >/dev/null 2>&1 &
   sleep 1
   exit 1
fi
