#!/bin/bash -x
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# stop on error
set -e

rm -rf ${BUILD_ROOT} || true
mkdir ${BUILD_ROOT}
pushd ${BUILD_ROOT}

# change this to use a different install location
export INSTALL_BASE=${HOME}/oz

# build type
CMAKE_BUILD_TYPE="Debug"
#CMAKE_BUILD_TYPE="RelWithDebInfo"

# libzmq
rm -rf libzmq || true
git clone https://github.com/nyfix/libzmq.git
pushd libzmq
git checkout nyfix
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} .. &&  make && make install
popd

# OpenMAMA
rm -rf OpenMAMA || true
git clone https://github.com/nyfix/OpenMAMA.git
pushd OpenMAMA
git checkout nyfix
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DWITH_UNITTEST=ON .. &&  make && make install
popd

# OpenMAMA-omnm
rm -rf OpenMAMA-omnm || true
git clone https://github.com/nyfix/OpenMAMA-omnm.git
pushd OpenMAMA-omnm
git checkout nyfix
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DMAMA_ROOT=${INSTALL_BASE} .. &&  make && make install
popd

# OZ
pushd ${SCRIPT_DIR}/..
rm -rf build || true
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_BASE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DMAMA_ROOT=${INSTALL_BASE} -DZMQ_ROOT=${INSTALL_BASE} .. &&  make && make install
popd

popd
