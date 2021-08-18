#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh
source ${SCRIPT_DIR}/oz-nsd.sh


${SCRIPT_DIR}/stop-nsd.sh

nohup "${SCRIPT_DIR}/nsd.sh" >/dev/null 2>&1 &
sleep 1
