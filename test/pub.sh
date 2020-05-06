#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

[[ -z ${MAMA_TPORT_PUB} ]] && echo "Must set MAMA_TPORT_PUB!" && exit 1
[[ -z ${MAMA_MW} ]] && echo "Must set MAMA_MW!" && exit 1

`which mamapublisherc` -tport ${MAMA_TPORT_PUB} -m ${MAMA_MW} $*
