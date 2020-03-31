
# Getting Started
This document will help you get started using OZ, the OpenMAMA/ZeroMQ middleware bridge.

The approach we've taken is to provide a scripted "cookbook" which eliminates the extensive "yak shaving" that is all too common when trying to get started with a new open-source project.

## Installing dependencies

In addition to a minimal install, the following additional packages are required:

Package | Required by
------- | -----------
gcc, gcc-c++ | all
git | all
cmake | all
epel-release | (for gtest-devel)
libxml2-devel | OpenMAMA
flex | OpenMAMA
gtest-devel | OpenMAMA, OpenMAMA-omnm
apr-devel | OpenMAMA
libuuid-devel | OpenMAMA, OZ
libevent-devel | OpenMAMA, OZ
uuidd | OZ

The `install-deps.sh` script, located in the `test` directory, can be used to install the above.  (Note that installs must be done as root).  

## Re: uuid's
OZ uses uuid's to uniquely identify nodes in the network.  To ensure that uuid's are unique, the code calls `uuid_generate_time_safe`, which requires that the uuidd daemon be running.  To start the daemon if it's not already running:

### RH/CentOS 7
```
sudo systemctl start uuidd
```

### RH/CentOS 6
```
sudo service uuidd start
```

If the uuidd daemon is not running, you will get an error similar to the following when attempting to run any OZ binaries:

```
13:31:19.288102|zmqBridgeMamaTransport_create:Failed to generate safe wUuid (transport.c:156)
```

## Building from source

Once the OS dependencies are installed, the next step is to build the components from source.  The `build-all.sh` script in the `test` directory handles that for you.

The script first sources the `setenv.sh` script, which defines the following environment variables:

Variable | Use
------- | -----------
BUILD_ROOT | Directory where required packages are downloaded.  Defaults to `/tmp/buildtemp`.
INSTALL_BASE | Directory where required packages are installed.  Defaults to `/tmp/oz`.

This makes it possible to build and install the packages in a non-default location -- this is especially helpful if you do not have write access to e.g., `/usr/local`.

> Note that both `BUILD_ROOT` and `INSTALL_BASE` will be deleted and re-created by the script -- DO NOT set these to a directory with files that you care about.

## Running OpenMAMA unit tests
The first line of defense for any OpenMAMA bridge implementation is the built-in unit tests.  OZ includes a script that can be used to run relevant unit tests automatically:

```
cd test
./ut.sh
```

Since OZ is intended to typically be run in "naming" mode, the `ut.sh` script will check if naming mode is configured -- if so, it will check that the `nsd` process is running, and will start (and stop) it automatically if needed.

The output from the unit tests can be a bit verbose -- to skinny it down you can execute the script like so:

```
./ut.sh 2>/dev/null | egrep "==========|PASSED|FAILED"
[==========] Running 34 tests from 5 test cases.
[==========] 34 tests from 5 test cases ran. (14006 ms total)
[  PASSED  ] 34 tests.
[==========] Running 201 tests from 18 test cases.
[  FAILED  ] MamaDictionaryTestC.LoadFromFileAndWriteToMsg (3 ms)
[==========] 201 tests from 18 test cases ran. (12981 ms total)
[  PASSED  ] 200 tests.
[  FAILED  ] 1 test, listed below:
[  FAILED  ] MamaDictionaryTestC.LoadFromFileAndWriteToMsg
 1 FAILED TEST
[==========] Running 641 tests from 56 test cases.
[==========] 641 tests from 56 test cases ran. (349 ms total)
[  PASSED  ] 641 tests.
[==========] Running 478 tests from 72 test cases.
[==========] 478 tests from 72 test cases ran. (221 ms total)
[  PASSED  ] 478 tests.
```

### Running the unit tests with Qpid
For comparison purposes, you may want to run the unit tests with OpenMAMA's builtin Qpid transport.

```
cd test
source qpid.sh
./ut.sh
```

## Running OpenMAMA examples

### Publish/subscribe

First we start the nsd:

```
cd test
source setenv.sh
source oz-nsd.sh
./nsd.sh
3/31 15:18:20.417985|main|4086-7fbcb718c7c0|INFO(0,0) nsd running at tcp://127.0.0.1:5756|nsd.c(109)
```

From another terminal session:

```
cd test
source setenv.sh
source oz-nsd.sh
./sub.sh
...
mamasubscriberc: Created inbound subscription.
...

```

From another terminal session:

```
cd test
source setenv.sh
source oz-nsd.sh
./pub.sh
...
Created inbound subscription.
...
3/31 15:20:39.712429|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 1 to MAMA_TOPIC {{MdMsgType[1]=1,MdMsgStatus[2]=0,MdSeqNum[10]=1,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
3/31 15:20:40.212552|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 2 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=2,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
3/31 15:20:40.712589|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 3 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=3,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
3/31 15:20:41.212964|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 4 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=4,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
3/31 15:20:41.713553|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 5 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=5,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
3/31 15:20:42.214053|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 6 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=6,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
3/31 15:20:42.714598|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 7 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=7,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
3/31 15:20:43.215114|publishMessage|4945-7fdea1823780|INFO(0,0) Publishing message 8 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=8,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
^C
```

Going back to the subscriber, you'll see the messages from the publisher:

```
...
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    1
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    1
              MdFeedHost    12               STRING           MAMA_TOPIC
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    2
              MdFeedHost    12               STRING           MAMA_TOPIC
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    3
              MdFeedHost    12               STRING           MAMA_TOPIC
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    4
              MdFeedHost    12               STRING           MAMA_TOPIC
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    5
              MdFeedHost    12               STRING           MAMA_TOPIC
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    6
              MdFeedHost    12               STRING           MAMA_TOPIC
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    7
              MdFeedHost    12               STRING           MAMA_TOPIC
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                    8
              MdFeedHost    12               STRING           MAMA_TOPIC
...              
```

### Producer/consumer

TBD


