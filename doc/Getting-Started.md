
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
source qpid-pubsub.sh
./ut.sh
```
## Where to go from here
If the unit tests look good, you might want to proceed to [run the OpenMAMA examples programs](RunningExamples.md).

If you're interested in performance (and who isn't), you can also check out the [OpenMAMA performance tests](Performance.md
).
#### See also
[OpenMAMA Build Instructions](https://openmama.github.io/openmama_build_instructions.html)

[OpenMAMA Unit Testing](https://openmama.github.io/openmama_unit_testing.html)

[OpenMAMA Qpid Bridge](https://openmama.github.io/openmama_qpid_bridge.html)

[ZeroMQ installation](https://github.com/zeromq/libzmq/blob/master/INSTALL)
