#!/bin/bash -x

# stop on error
set -e

if [[ $(lsb_release -i) =~ "Ubuntu" ]]; then
   sudo apt install git
   sudo apt install gcc g++
   sudo apt install cmake
   sudo apt install flex uuid-dev libevent-dev libapr1-dev libncurses-dev libqpid-proton11-dev
else
   # assume el
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
   yum install libevent-devel
   yum install qpid-proton-c-devel qpid-cpp-server
fi