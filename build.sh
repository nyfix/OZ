#!/bin/bash -x

# this is defined as a function just so we can use local variables...
function setenv {
   local SCRIPT_DIR=$(cd `dirname $BASH_SOURCE` && pwd)                           # get current directory
   local BUILD_TYPE_PARAM;
   [[ -n ${BUILD_TYPE} ]] && BUILD_TYPE_PARAM="-b ${BUILD_TYPE}"
   local CONFIG_PARAM;
   [[ -n ${CONFIG} ]]     && CONFIG_PARAM="-c ${CONFIG}"
   source ${SCRIPT_DIR}/admin/devenv.sh ${BUILD_TYPE_PARAM} ${CONFIG_PARAM}
}

# get cmd line params
VERBOSE=""
SUFFIX=""
while getopts ':s::b:c:i:v' flag; do
  case "${flag}" in
    b) BUILD_TYPE="${OPTARG}"   ; export BUILD_TYPE ;;
    c) CONFIG="${OPTARG}"       ; export CONFIG ;;
    i) INSTALL_BASE="${OPTARG}" ; export INSTALL_BASE ;;
    v) VERBOSE="VERBOSE=1"      ;;
    s) SUFFIX="${OPTARG}"
  esac
done
shift $((OPTIND - 1))
# certain build types imply a particular configuration
[[ ${BUILD_TYPE} == *san ]] && export CONFIG=clang
# if build type not specified, assume "dev"
[[ -z ${BUILD_TYPE} ]] && BUILD_TYPE=dev
# static linking not supported w/tsan
#UNITTEST=y;[[ ${BUILD_TYPE} == tsan ]] && UNITTEST=n

setenv

# INSTALL_BASE and BUILD_TYPE must be specified (or set in environment)
[[ -z ${INSTALL_BASE} ]] && echo "No INSTALL_BASE specified" && exit 1
[[ -z ${BUILD_TYPE} ]]   && echo "No BUILD_TYPE specified" && exit 1

# debug/release
CMAKE_BUILD_TYPE="Debug"
if [[ ${BUILD_TYPE} == "release" ]] ; then
   CMAKE_BUILD_TYPE="RelWithDebInfo"
fi

# set install location
INSTALL_PREFIX="${INSTALL_BASE}/${PROJECT_NAME}/${PROJECT_VERSION}${SUFFIX}/${BUILD_TYPE}"

# delete old install
[[ -n ${INSTALL_PREFIX} && -d ${INSTALL_PREFIX} ]] && rm -rf ${INSTALL_PREFIX}

# delete old build
rm -rf build
mkdir build
cd build

# do the build
cmake \
   -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
   -DCMAKE_INSTALL_PREFIX=${INSTALL_PREFIX} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
   -DMAMA_SRC=${OPENMAMA_SOURCE} -DMAMA_ROOT=${OPENMAMA_INSTALL} \
   -DEVENT_ROOT=${LIBEVENT_INSTALL} \
   -DZMQ_ROOT=${LIBZMQ_INSTALL} \
   ..
make ${VERBOSE} && make ${VERBOSE} install
rc=$?
[[ $rc -ne 0 ]] && exit $rc

# copy source to facilitate debugging
mkdir -p ${INSTALL_PREFIX}/src
cp -rp ../src ${INSTALL_PREFIX}/
