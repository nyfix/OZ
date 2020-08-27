
# Getting Started
This document will help you get started using OZ, the OpenMAMA/ZeroMQ middleware bridge.

The approach we've taken is to provide a scripted "cookbook" which eliminates the extensive "yak shaving" that is all too common when trying to get started with a new open-source project.

## The short version
Do you feel lucky today?  Well, do you?

Seriously, in many cases you should be able to simply run the following and it will "just work":

```
git clone https://github.com/nyfix/OZ.git
cd OZ/test
./build-all.sh
./ut.sh
```

If that doesn't work, or if you're just naturally curious, keep reading for the details.

## Installing dependencies

In addition to a minimal install, the following additional packages are required:

Package | Required by
------- | -----------
gcc, gcc-c++ | all
git | all
cmake | all
epel-release | (for gtest-devel)
ncurses-devel | OpenMAMA
libxml2-devel | OpenMAMA
flex | OpenMAMA
gtest-devel | OpenMAMA, OpenMAMA-omnm
apr-devel | OpenMAMA
libuuid-devel | OpenMAMA, OZ
libevent-devel | OpenMAMA, OZ
uuidd | OZ

The `install-deps.sh` script, located in the `test` directory, can be used to install the above.  (Note that installs must be done as root). 

### Mac support

```
brew install gnutls
```
 

## Re: uuid's
OZ uses uuid's to uniquely identify nodes in the network.  To ensure that uuid's are unique, the code calls `uuid_generate_time_safe`, which requires that the uuidd daemon be running.  To start the daemon if it's not already running (following must be run as root):

### RH/CentOS 7
```
systemctl start uuidd
```

### RH/CentOS 6
```
service uuidd start
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

## Environment variables
We've established a convention for environment variables that are required with OpenMAMA -- this initially came about because of a requirement to set these values without specifying on them on the command line, and it has proven to be convenient enough to stick with it.

Variable | Default | Use
------- | -----| ------
MAMA_MW | zmq| Specifies the middleware transport to load.
MAMA_NSD_ADDR| 127.0.0.1 | Specifies the address of the nsd process.
MAMA_NSD_PORT| 5756 | Specifies the port at which the nsd listens.  Defaults to 5756
MAMA_PAYLOAD | omnmmsg| Specifies the payload library to load.
MAMA_TPORT_PUB | oz | Specifies the transport name for "publishers".  
MAMA_TPORT_SUB | oz | Specifies the transport name for "subscribers".

The convention of using different transport names for "publishers" and "subscribers" is a hold-over from the [OpenMAMA examples](https://openmama.github.io/openmama_quick_start_guide_running_openmama_apps.html), and is completely arbitrary -- a "subscriber" can still publish messages, and a "publisher" can still subscribe.  

With OZ these differences are meaningless in any case, at least in [naming mode](Naming-Service.md), since transports use the naming service to get addressing information. 


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
source qpid-pubsub.sh
source qpidmsg.sh
./ut.sh
```

## Where to go from here
If you're already familiar with OpenMAMA, you might want to proceed to [run the OpenMAMA example programs](Running-Examples.md).  We've supplied scripts to make this process a bit easier, but it can nevertheless be a bit fiddly, and assumes at least some familiarity with OpenMAMA.

We've also created a kinder, gentler (read, simpler) set of samples using modern C++ that can be found [here](../examples/Readme.md).  This may be a better choice if you are not already familiar with OpenMAMA.

If you're interested in performance (and who isn't), you can also check out the [OpenMAMA performance tests](Performance.md).


#### See also
[OpenMAMA Build Instructions](https://openmama.github.io/openmama_build_instructions.html)

[OpenMAMA Unit Testing](https://openmama.github.io/openmama_unit_testing.html)

[OpenMAMA Qpid Bridge](https://openmama.github.io/openmama_qpid_bridge.html)

[ZeroMQ installation](https://github.com/zeromq/libzmq/blob/master/INSTALL)
