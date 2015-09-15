# OpenMAMA ZeroMQ Middleware Bridge

## Overview

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

## Functionality

The current version of the bridge actually has comprehensive middleware bridge
functionality. It has:

* Support for any serializable / deserializable MAMA Message payload bridge
* Reasonably good performance (more details to be confirmed...)
* Request / Reply Functionality
* Basic Publish / Subscribe
* Market Data Publish / Subscribe
* ZeroMQ TCP transport compatibility

In layman's terms, this means that it is fully compatible with:

* mamapublisherc / mamasubscriberc
* mamapublisherc / mamainboxc
* capturereplayc / mamalistenc

Which is pretty much all of the MAMA functionality that most people care about.

## Build Instructions

*NB: This is very much in development and I will always be developing on the
latest version of Fedora. If I have broken an OS that you use, please let me
know.*

### Supported Platforms

* RHEL / CentOS 5
* RHEL / CentOS 6
* Windows (coming soon)

### Dependencies

The bridge depends on:

* MAMA / OpenMAMA
* Libevent
* Libuuid
* Scons

Until dynamic bridge loading is supported, you will also need to build against
my github fork of OpenMAMA (https://github.com/fquinner/OpenMAMA) on the
`feature-omzmq` branch which contains the middleware name and enum as well as
a couple of submitted-but-not-merged-yet openmama patches to allow the qpid
proton payload to work with other middlewares
(http://lists.openmama.org/pipermail/openmama-dev/2015-August/001582.html).
I will add the enum upstream at some point in the future too, but I plan on
using this bridge to help test the dynamic loading, so I'll not add the enum
to OpenMAMA master until that is ready.

### Building

If you have all the prerequisites, the build process should be pretty
straightforward:

    scons --with-mamasource=PATH --with-mamainstall=PATH

## Usage Instructions

After building, you will have a `libmamazmqimpl.so` library created. Add the
directory containing this library to your `LD_LIBRARY_PATH` and run your
applications with `-m zmq` to use the bridge. Example mama.properties
transport configuration parameters are included in the code in the `config`
folder.

## Related Projects

I am also in the process of cleaning up a native payload implementation  which
was created to allow performance tests which are actually useful (qpid proton
payloads are too slow to actually test the capacity of this bridge). When that
project comes online, it will also be linked here.

* [OpenMAMA](http://openmama.org)
* [ZeroMQ](http://zeromq.org)
* *OMNM (OpenMAMA Native Message) - coming soon*

## Blog

If you're interested in the thought process behind this or the ramblings of the
author, you can shoot on over to [my blog page](http://fquinner.github.io).
