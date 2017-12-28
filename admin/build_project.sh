#!/bin/bash -xv

source ${BUILD_DIR}/admin/devenv.buildmaster.sh $*

[[ -n ${BUILD_NUMBER} ]] && SUFFIX="-s -${BUILD_NUMBER}"

# build release version
./build.sh -v ${SUFFIX}
