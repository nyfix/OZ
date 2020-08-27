#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

killall "${INSTALL_BASE}/bin/nsd"
