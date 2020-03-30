#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

`which mamapublisherc` -tport ${MAMA_TPORT_PUB} -m ${MAMA_MW} $*
