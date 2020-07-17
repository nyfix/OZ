#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

BRANCH="$@"
[[ -z "${BRANCH}" ]] && BRANCH="-b nyfix"

# stop on error
set -e

# build type
CMAKE_BUILD_TYPE="Debug"
#CMAKE_BUILD_TYPE="RelWithDebInfo"

# build flags
CMAKE_C_FLAGS="-fno-omit-frame-pointer -DNYFIX_LOG"
CMAKE_CXX_FLAGS="-fno-omit-frame-pointer -DNYFIX_LOG"


# OZ
pushd ${SCRIPT_DIR}/..
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DMAMA_ROOT=${INSTALL_BASE} -DZMQ_ROOT=${INSTALL_BASE} \
   -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS}" \
   ..
make; make install
popd

popd
