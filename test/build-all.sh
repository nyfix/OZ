#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# stop on error
set -e

# clean out build, install directories
rm -rf ${BUILD_ROOT} && mkdir ${BUILD_ROOT}
pushd ${BUILD_ROOT}
rm -rf ${INSTALL_BASE} && mkdir ${INSTALL_BASE}

# build type
CMAKE_BUILD_TYPE="Debug"
#CMAKE_BUILD_TYPE="RelWithDebInfo"

# build flags
CMAKE_C_FLAGS="-fno-omit-frame-pointer -DNYFIX_LOG"
CMAKE_CXX_FLAGS="-fno-omit-frame-pointer -DNYFIX_LOG"

# libzmq
rm -rf libzmq || true
git clone -b nyfix https://github.com/nyfix/libzmq.git
pushd libzmq
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
   -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS}" \
   ..
make; make install
popd

# OpenMAMA
rm -rf OpenMAMA || true
git clone -b nyfix https://github.com/nyfix/OpenMAMA.git
pushd OpenMAMA
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DWITH_UNITTEST=ON \
   -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS}" \
   ..
make; make install
popd

# OpenMAMA-omnm
rm -rf OpenMAMA-omnm || true
git clone -b nyfix https://github.com/nyfix/OpenMAMA-omnm.git
pushd OpenMAMA-omnm
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DMAMA_ROOT=${INSTALL_BASE} \
   -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS}" -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS}" \
   ..
make; make install
popd

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
