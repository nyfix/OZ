# OpenMAMA ZeroMQ Middleware Bridge

[![Join the chat at https://gitter.im/fquinner/OpenMAMA-zmq](https://badges.gitter.im/fquinner/OpenMAMA-zmq.svg)](https://gitter.im/fquinner/OpenMAMA-zmq?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Build Status](https://travis-ci.org/fquinner/OpenMAMA-zmq.svg?branch=master)](https://travis-ci.org/fquinner/OpenMAMA-zmq)

## Functionality

This project now has complete MAMA middleware bridge functionality.
It passes all of the OpenMAMA middleware unit tests and provides:

* Support for point to point, fan-in and fan-out messaging
* Support for **any** serializable / deserializable MAMA Message payload bridge
* Significant performance improvement over qpid
* Request / Reply Functionality
* Basic Publish / Subscribe
* Market Data Publish / Subscribe
* ZeroMQ TCP transport compatibility
* ZeroMQ PGM (multicast) transport compatibility
* ZeroMQ IPC (named pipes) transport compatibility

You can expect the pub / sub / request / reply example apps to work great out
of the box including:

* mamapublisherc / mamasubscriberc
* mamapublisherc / mamainboxc
* capturereplayc / mamalistenc

## Releases

We now provide binary releases - see 
https://github.com/fquinner/OpenMAMA-zmq/releases for a list of binaries
available. If we have missed a platform that you are using, please let us
know.

## Usage Instructions

### Linux
After building or grabbing a binary release, you will have a
`libmamazmqimpl.so` file created. Add the
directory containing this library to the `LD_LIBRARY_PATH` environment
variable and run your applications with `-m zmq` to use the bridge.

### Windows
After building or grabbing a binary release, you will have a
`libmamazmqimplmd.dll` file created. Add the directory containing this
library to the `PATH` environment variable or add it to the executable's
directory to allow it to load and run your applications with `-m zmq` to
use the bridge.

### Setting up a transport

Check out the [mama.properties](config/mama.properties) file for details on
the various transport options available and how to set them up.
You can also use a forwarder as
detailed [here](http://fquinner.github.io/2015/12/05/openmama-and-zeromq-fanout/).

It's worth mentioning you can also override selected zmq socket settings
directly as listed in the configuration table below.

You may set a configuration parameter by setting `mama.zmq.transport.<transportname>.<setting>`
either programatically or through `mama.properties` where `setting` is one of the
parameters listed below.

|Configuration Setting|Brief Description|Default Value in Bridge|
|---|---|---|
|msg_pool_size|Number of elements to store in the transport's message pool|1024|
|msg_node_size|Initial size of each memory node in the transport's message pool|4096
|outgoing_url_0, outgoing_url_1 .. outgoing_url_N|Multiple URLs to send all publisher messages to including published data and subscription requests (see ZMQ URI documentation)|'tcp://\*:5557' if transport name is 'sub', 'tcp://*:5556' if transport name is 'pub'|
|incoming_url_0, incoming_url_1 .. incoming_url_N|Multiple URLs to receive messages from including subscription data and replies (see ZMQ URI documentation)|'tcp://127.0.0.1:5556' if transport name is 'sub', 'tcp://127.0.0.1:5557' if transport name is 'pub'|
|zmq_sndhwm|High watermark for outbound messages|0 (Unlimited)|
|zmq_rcvhwm|High watermark for inbound messages|0 (Unlimited)|
|zmq_identity|Socket identifier|NULL|
|zmq_affinity|Set I/O thread affinity (bitmap)|0 (Unbinded)|
|zmq_sndbuf|Kernel transmit buffer size|OS Default|
|zmq_rcvbuf|Kernel receive buffer size|OS Default|
|zmq_reconnect_ivl|Reconnection interval (milliseconds)|100|
|zmq_reconnect_ivl_max|Maximum reconnection interval during exponential backoff policy|0 (Only use reconnection interval ad infinum)|
|zmq_backlog|Maximum length of the queue of outstanding connections|100|
|zmq_maxmsgsize|Maximum message size to be transferred|-1 (Unlimited)|
|zmq_rcvtimeo|Maximum time before a recv operation returns with EAGAIN (milliseconds)|10|
|zmq_sndtimeo|Maximum time before a send operation returns with EAGAIN (milliseconds)|-1 (Unlimited)|
|zmq_rate|Maximum multicast data rate (kbps)|1,000,000|

Note that any settings made here will apply to all sockets that are created
by the transport. For more details on the parameters beginning with `zmq_`, please
refer to the [ZeroMW socket option documentation](http://api.zeromq.org/4-0:zmq-setsockopt).

## Payload Selection

This middleware bridge uses the qpid payload bridge by default. If you want
to use the omnm payload (less functionality but _much_ faster than qpid),
you can have a look at [the omnm github page](https://github.com/fquinner/OpenMAMA-omnm) to find
to see what's involved.

## Build Instructions

### Supported Platforms

* RHEL / CentOS
* Fedora
* Ubuntu
* Windows

### Dependencies

The bridge depends on:

* MAMA / OpenMAMA (2.4.0+ / 6.0.7+ / 6.1.x)
* Libevent (1.x)
* Scons (will also require Python)
* Libuuid (only on Linux)

### Building

If you have all the prerequisites, the build process should be pretty
straightforward:

    scons --with-mamasource=PATH --with-mamainstall=PATH

## Project Goals

This ethos of this project is to:

* Create a fully functional ZeroMQ bridge which introduces minimal latency and
  maximum throughput when compared with a native ZeroMQ implementation.
* Create a bridge which is fully conformant with the latest OpenMAMA acceptance
  and unit tests.
* Give anything which would be useful for 'any old middleware bridge' back to
  the OpenMAMA core project.

*NB: This project is MIT Licensed and in no way affiliated with nor supported
by the OpenMAMA project or SR Labs - if you find any issues, please report to
this project via github.*

## Related Projects

* [OpenMAMA](http://openmama.org)
* [ZeroMQ](http://zeromq.org)
* [OpenMAMA Native Message (OMNM)](https://github.com/fquinner/OpenMAMA-omnm)

## Blog

If you're interested in the thought process behind this or the ramblings of the
author, you can shoot on over to [my blog page](http://fquinner.github.io).
