#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

[[ -z ${MAMA_TPORT_SUB} ]] && echo "Must set MAMA_TPORT_SUB!" && exit 1
[[ -z ${MAMA_MW} ]] && echo "Must set MAMA_MW!" && exit 1

`which mamasubscriberc` -tport ${MAMA_TPORT_SUB} -m ${MAMA_MW} $*
