#!/bin/bash -x

SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)
source ${SCRIPT_DIR}/setenv.sh

# stop on error
set -e

if [[ ${OS} =~ "darwin" ]]; then
   brew bundle --verbose --no-upgrade --file=- <<EOS
   brew "apr-util"
   brew "cmake"
   brew "ossp-uuid"
   brew "qpid-proton"
   brew "gnutls"
   brew "googletest"
   brew "pidof"
EOS
elif [[ ${OS} =~ "Ubuntu" ]]; then
   sudo apt install git
   sudo apt install gcc g++
   sudo apt install cmake
   sudo apt install flex
   sudo apt install uuid-dev
   sudo apt install libapr1-dev
   sudo apt install libncurses-dev
   sudo apt install libqpid-proton11-dev
elif [[ ${OS} =~ "CentOS" || -z ${OS} ]]; then
   # assume RH/CentOS (?!)
   yum install gcc gcc-c++
   yum install git
   yum install cmake
   yum install epel-release
   yum install ncurses-devel
   yum install libxml2 libxml2-devel
   yum install flex
   yum install gtest-devel
   yum install apr-devel
   yum install libuuid-devel uuidd
   yum install qpid-proton-c-devel qpid-cpp-server
   yum install redhat-lsb-core
else
   echo "Sorry, dont know how to install dependencies for ${OS}"
   exit 1
fi