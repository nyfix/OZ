# Changes from upstream
There are several places where OZ does things somewhat differently than the upstream packages that it is either based on or depends on.

We are hopeful that, with help from the OpenMAMA and ZeroMQ communities, these differences can be resolved over time.

# Platform Support
Both OpenMAMA and ZeroMQ support a number of different platforms, including various flavors of Linux, and Windows.  Currently, OZ is only supported on RedHat/CentOS Linux version 6 and above.

# Versions/Repos
As of this writing, OZ is compatible with the latest versions of upstream packages (libzmq, OpenMAMA, OpenMAMA-omnm).  That won't always be the case, however -- development takes place at different times and different rates for each package.  To accomodate that, OZ is synchronized against forks of upstream packages.

Package | Repo
-------- | ---------- 
libzmq | <https://github.com/nyfix/libzmq.git>
OpenMAMA | <https://github.com/nyfix/OpenMAMA.git>
OpenMAMA-omnm | <https://github.com/nyfix/OpenMAMA-omnm.git> 
OZ | <https://github.com/nyfix/OZ.git> 

For each of the repos above, the following convention is used to specify branches:

Branch |  Description
-------- | ---------- 
master | Tracks upstream.
staging | Used to submit patches against upstream, and for development builds.
nyfix | Current production version. 

# libzmq
Since OZ makes use of CLIENT/SERVER sockets (for IPC), it must be built with "draft mode" enabled.

# OpenMAMA

## mama_log
OpenMAMA applications typically use the `mama_log` function to write messages to the application log.  

The forked version of OpenMAMA redefines `mama_log` as a macro in order to capture additional information at the call site, including function name, file name and line number.  When used with the forked version of OpenMAMA, the `MAMA_LOG` macro resolves to this redefined `mama_log` macro.  

To build with this modification, define `NYFIX_LOG` as part of the build.

# OZ

## Formatting Conventions
Originally the formatting of OZ source code was maintained according to [standard OpenMAMA conventions](https://openmama.github.io/openmama_coding_standards.html) to make it easier to submit changes to the original project.  When it was later decided to fork the project and develop independently, there was no longer a need to maintain the arguably outdated (i.e., K&R-style) OpenMAMA conventions, and the original code has been reformatted to conform more closely to NYFIX standards.  

<!-- TODO: create an .astylerc and/or .clangtidy file to reformat the code -->

# Opaque Pointers (void*'s)
OpenMAMA goes to great lengths to ensure that internal data structures are only visible to the compilation units in which they are defined, typically by defining those data structures in .c files, rather than in .h files, and using `void*`'s to hide the implementation details from other parts of the code.  

The benefit of this approach is to make it literally impossible for another compilation unit to "peek" at the internal representation of the object (i.e., by casting the opaque pointer to a concrete type), since the definition of the concrete type is unknown outside its translation unit.

OZ takes a somewhat more relaxed approach -- most objects are defined in header files that are included in the individual translation units, and objects are typically referred to by their concrete types, rather than as `void*`'s.  This avoids a whole class of potential problems caused by wayward casts, and as an added benefit greatly simplifies debugging.  (You can read one person's opinions on the subject [here](http://btorpey.github.io/blog/2014/09/23/into-the-void/) ;-)

Opaque pointers are still used by functions that implement the OpenMAMA API, but internal functions use pointers to concrete types.

# Public vs. private functions
For the most part, OZ adheres to the convention that "public" functions (functions defined as part of the OpenMAMA API) are named using the form "object_operation", while "private" functions (not part of the API) are named using "objectImpl_operation".

# Thread Safety
ZeroMQ is very fussy about how to properly access sockets in a multi-threaded environment, and it is quite easy to get that wrong, with serious consequences (e.g., `SEGV`).  

OZ is completely thread-safe internally, and makes it difficult to misuse ZeroMQ in a non-thread-safe-manner.

- SUB sockets are owned by the main dispatch thread, and are never accesssed concurrently with the dispatch thread.
 - Any operations that an application needs to be perform on SUB sockets (e.g., subscribe, unsubscribe) are accomplished by sending a message to the main dispatch thread, which performs those operations in a thread-safe manner.
- PUB sockets are protected by mutexes, which bracket `zmq_send` operations.
- Internal state that can be accessed by multiple threads use atomic operations to ensure that there are no data races.

