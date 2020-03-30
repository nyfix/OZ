#!/bin/bash -x

SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

`which mamasubscriberc` -tport ${MAMA_TPORT_SUB} -m ${MAMA_MW} $*
