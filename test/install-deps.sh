#!/bin/bash -x

# stop on error
set -e

yum install gcc gcc-c++
yum install git
yum install cmake
yum install epel-release
yum install libxml2 libxml2-devel
yum install flex
yum install gtest-devel
yum install apr-devel
yum install libuuid-devel uuidd
yum install libevent-devel
yum install qpid-proton-c-devel qpid-cpp-server
