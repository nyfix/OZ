#!/bin/bash -x

PACKAGE=OpenMAMA-zmq
VERSION=nyfix
REPO=OpenMAMA-zmq

# get cmd line params
VERBOSE=""
INSTALL_BASE=${HOME}/install
while getopts ':b:c:i:v' flag; do
  case "${flag}" in
    b) BUILD_TYPE="${OPTARG}"   ; export BUILD_TYPE ;;
    c) CONFIG="${OPTARG}"       ; export CONFIG ;;
    i) INSTALL_BASE="${OPTARG}" ; export INSTALL_BASE ;;
    v) VERBOSE="VERBOSE=1"      ;;
  esac
done
shift $((OPTIND - 1))
# certain build types imply a particular configuration
[[ ${BUILD_TYPE} == *san ]] && export CONFIG=clang
[[ -z ${BUILD_TYPE} ]] && BUILD_TYPE=dev

CMAKE_BUILD_TYPE="Debug"
if [[ ${BUILD_TYPE} == "release" ]] ; then
   CMAKE_BUILD_TYPE="RelWithDebInfo"
fi

OPENMAMA_INSTALL=${INSTALL_BASE}/OpenMAMA/${VERSION}/${BUILD_TYPE}
OPENMAMA_SOURCE=${OPENMAMA_INSTALL}/src
ZEROMQ_INSTALL=${INSTALL_BASE}/libzmq/${VERSION}/${BUILD_TYPE}

INSTALL_PREFIX=${INSTALL_BASE}/${PACKAGE}/${VERSION}/${BUILD_TYPE}

# delete old install
rm -rf ${INSTALL_PREFIX}

# copy source to facilitate debugging
mkdir -p ${INSTALL_PREFIX}/src
cp -rp src ${INSTALL_PREFIX}/

# delete old build
rm -rf build
mkdir build
cd build


cmake  -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
   -DMAMA_SRC=${OPENMAMA_SOURCE} -DMAMA_ROOT=${OPENMAMA_INSTALL} -DZMQ_ROOT=${ZEROMQ_INSTALL} \
   ..
make ${VERBOSE} && make ${VERBOSE} install
