#!/bin/bash

#export ROOT=${HOME}
export ROOT=/tmp

# change this to use a different build location
export BUILD_ROOT=${ROOT}/buildtemp

# change this to use a different install location
export INSTALL_BASE=${ROOT}/oztest
export PATH=${INSTALL_BASE}/bin:$PATH
export LD_LIBRARY_PATH=${INSTALL_BASE}/lib64:${INSTALL_BASE}/lib:$LD_LIBRARY_PATH
