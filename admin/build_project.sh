#!/bin/bash -xv

source ${BUILD_DIR}/admin/devenv.buildmaster.sh $*

[[ -n ${BUILD_NUMBER} ]] && export SUFFIX="-${BUILD_NUMBER}"

# build release version
./build.sh -v
