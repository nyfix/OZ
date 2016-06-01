# OpenMAMA ZeroMQ Middleware Bridge

[![Join the chat at https://gitter.im/fquinner/OpenMAMA-zmq](https://badges.gitter.im/fquinner/OpenMAMA-zmq.svg)](https://gitter.im/fquinner/OpenMAMA-zmq?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

## Functionality

This project now has complete MAMA middleware bridge functionality.
It passes all of the OpenMAMA middleware unit tests and provides:

* Support for any serializable / deserializable MAMA Message payload bridge
* Reasonably good performance (more details to be confirmed...)
* Request / Reply Functionality
* Basic Publish / Subscribe
* Market Data Publish / Subscribe
* ZeroMQ TCP transport compatibility
* ZeroMQ PGM (multicast) transport compatibility
* ZeroMQ IPC (named pipes) transport compatibility

You can expect the pub / sub / request / reply example apps to work great out
of the box:

* mamapublisherc / mamasubscriberc
* mamapublisherc / mamainboxc
* capturereplayc / mamalistenc

## Build Instructions

*NB: This is very much in development and I will always be developing on the
latest version of Fedora. If I have broken an OS that you use, please let me
know.*

### Supported Platforms

* RHEL / CentOS 5+
* Windows 7+

### Dependencies

The bridge depends on:

* MAMA / OpenMAMA
* Libevent
* Libuuid (Linux only)
* Scons

As of the 2.4.0 release of OpenMAMA, there is no longer a requirement to
build this library off my own special fork of OpenMAMA. Instead thanks to
dynamic bridge loading support, you can now build this off the master
branch of OpenMAMA. I may also make binary releases available soon.

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

If you want to use the omnm payload (less functionality but faster than qpid),
you can have a look at
[the omnm github page](https://github.com/fquinner/OpenMAMA-omnm) to find
out how.

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
