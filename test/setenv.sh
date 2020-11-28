#!/bin/bash

#export ROOT=${HOME}
export ROOT=/tmp

# change this to use a different build location
export BUILD_ROOT=${ROOT}/buildtemp

# figure out OS
if [[ ${OSTYPE} =~ "darwin" ]]; then
   export OS=${OSTYPE}
else
   export OS=$(lsb_release -i 2>/dev/null)
fi

if [[ ${OS} =~ darwin ]]; then
   LIBPATH=DYLD_LIBRARY_PATH
else
   LIBPATH=LD_LIBRARY_PATH
fi

# change this to use a different install location
[[ -z ${INSTALL_BASE} ]] && export INSTALL_BASE=${ROOT}/oztest
# set paths
export PATH=${INSTALL_BASE}/bin:$PATH
eval "export ${LIBPATH}=${INSTALL_BASE}/lib64:${INSTALL_BASE}/lib:${!LIBPATH}"

