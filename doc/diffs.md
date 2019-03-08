# Application Logging
OpenMAMA applications typically use the `mama_log` function to write messages to the application log.  

The forked version of OpenMAMA redefines `mama_log` as a macro in order to capture additional information at the call site, including function name, file name and line number.  When used with the forked version of OpenMAMA, the `MAMA_LOG` macro resolves to this redefined `mama_log` macro.  

If you are using OZ with a version of OpenMAMA that does *not* redefine `mama_log`, you can `#undef USE_MAMA_LOG`, which will cause OZ to instead delegate `MAMA_LOG` calls to the internal `mama_log_helper` function in order to output the function, file name and line number.   


# Formatting Conventions
Originally the formatting of OZ source code was maintained according to [standard OpenMAMA conventions](https://openmama.github.io/openmama_coding_standards.html) to make it easier to submit changes to the original project.  When it was later decided to fork the project and develop independently, there was no longer a need to maintain the arguably outdated (i.e., K&R-style) OpenMAMA conventions, and the original code has been reformatted to conform more closely to the standards used in our shop.  

<!-- TODO: create an .astylerc and/or .clangtidy file to reformat the code -->

# Opaque Pointers (void*'s)
OpenMAMA goes to great lengths to ensure that internal data structures are only visible to the compilation units in which they are defined, typically by defining those data structures in .c files, rather than in .h files, and using `void\*'s to hide the implementation details from other parts of the code.  

The benefit of this approach is to make it literally impossible for another compilation unit to "peek" at the internal representation of the object (i.e., by casting the opaque pointer to a concrete type), since the definition of the concrete type is unknown outside its translation unit.

OZ takes a somewhat more relaxed approach -- most objects are defined in header files that are included in the individual translation units, and objects are typically referred to by their concrete types, rather than as void\*'s.  This avoids a whole class of potential problems caused by wayward casts, and as an added benefit greatly simplifies debugging.

Opaque pointers are still used by functions that implement the OpenMAMA API, but internal functions use pointers to concrete types.

# Public vs. private functions
For the most part, OZ adheres to the convention that "public" functions (functions defined as part of the OpenMAMA API) are named using the form "object_operation", while "private" functions (not part of the API) are named using "objectImpl_operation".

# Thread Safety
ZeroMQ is quite fussy about how to properly access sockets in a multi-threaded environment, and the original bridge implementation largely ignored those issues, or left them to the application.

OZ is completely thread-safe internally, and makes it difficult to misuse ZeroMQ in a non-thread-safe-manner.

- SUB sockets are owned by the main dispatch thread, and are never accesssed concurrently with the dispatch thread.
- Any operations that an application needs to be perform on SUB sockets (e.g., subscribe, unsubscribe) are accomplished by sending a message to the main dispatch thread, which performs those operations in a thread-safe manner.
- PUB sockets are protected by mutexes, which bracket `zmq_send` operations.
- Internal state that can be accessed by multiple threads use atomic operations to ensure that there are no data races.


