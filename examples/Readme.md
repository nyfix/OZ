# OZ Examples

## Example programs

These are stripped-down versions of the examples included with OpenMAMA -- while they don't support options in the same way as the OpenMAMA examples, their relative brevity (around 10% of the size of the OpenMAMA examples), and their use of modern C++ constructs, should make the examples easier to use and understand.

> The examples use the classes described [below](#OZ-API) and implemented in the `ozimpl.h`/`ozimpl.cpp`files to greatly simplify the OpenMAMA API and significantly flatten its learning-curve. 


Program | Description
----- | -------------
pub.cpp | Publisher example.
sub.cpp | Subscriber example.  Demonstrates using an "events" class to process asynchronous callbacks.
wc.cpp | Sample subscriber using "wildcard" (POSIX regex) topics.
wc2.cpp | Similar to wc.cpp, but using WS-Topic style (hierarchical) topics.
req.cpp | Request/reply example, which also demonstrates manual lifetime management of objects.
rep.cpp | Sample progam to reply to requests, using `reply` class to wrap the original request.
req2.cpp | Request/reply example demonstrating how to wait for a synchronous reply, and how to multiply inherit from both `request` and `requestEvents` classes.
timer.cpp | Timer sample demonstrating manual lifetime management.
timer2.cpp | Similar to timer.cpp, but using automatic lifetime management (RAII).

## Running the examples
The example programs all default to using OZ as the transport bridge, omnm as the payload library, and "oz" as the name used for `mama.properties`.  In short, everything should "just work", with one important caveat: you need to make sure that the `nsd` process is running to handle [dynamic discovery](../doc/Naming-Service.md).

```
cd test
./start-nsd.sh
```

If the nsd is not running, you will get an error like this:

```
8/27 15:46:41.854016|zmqBridgeMamaTransportImpl_init|23449-7feb58e8d7c0|INFO(0,0) Bound publish socket to:tcp://127.0.0.1:36906 |transport.c(357)
8/27 15:46:51.761901|zmqBridgeMamaTransportImpl_start|23449-7feb58e8d7c0|CRIT(0,0) Failed connecting to naming service after 100 retries|transport.c(407)
8/27 15:46:51.761925|zmqBridgeMamaTransport_create|23449-7feb58e8d7c0|ERR(0,0) Error 9(STATUS_TIMEOUT)|transport.c(188)
8/27 15:46:51.761933|start|23449-7feb58e8d7c0|ERR(0,0) Error 9(STATUS_TIMEOUT)|ozimpl.cpp(25)
8/27 15:46:51.761939|main|23449-7feb58e8d7c0|ERR(0,0) Error 9(STATUS_TIMEOUT)|pub.cpp(26)
terminate called after throwing an instance of 'mama_status'
Aborted (core dumped)
```

To stop the nsd:

```
cd test
./stop-nsd.sh
```

With the `nsd` process running, you can execute any of the examples from the command line directly:

```
cd examples
source ../test/setenv.sh
source ../test/oz-nsd.sh
./pub
...
8/27 15:49:09.763739|zmqBridgeMamaTransportImpl_init|23831-7f4c912807c0|INFO(0,0) Bound publish socket to:tcp://127.0.0.1:31057 |transport.c(357)
topic=prefix/suffix,msg={name=value,num=1}
topic=prefix/suffix,msg={name=value,num=2}
topic=prefix/suffix,msg={name=value,num=3}
...
```

Each of the example programs can also take parameters on the command line (or from environment variables in some cases):

```
{program} {-m middleware} {-p payload} {-tport name} {topic} 
```

Param | Env Var | Default | Description
----- | ------- | ------- | ----
-m | MAMA_MW | zmq | Middleware transport to load.
-p | MAMA_PAYLOAD | omnmmsg | Payload library to load.
-tport | MAMA_TPORT_PUB or MAMA_TPORT_SUB | oz | Transport name for `mama.properties`
|||n/a|Topic to publish or subscribe to.

### Running the examples with Qpid
The examples also run with Qpid middleware and/or payload libraries.  

> The Qpid middleware bridge does NOT clean-up properly, and will often core at shutdown.  

## OZ API
The `oz` namespace defines several classes designed to provide a simple, easy-to-use introduction to OpenMAMA and OZ.  These classes greatly simplify the OpenMAMA API and significantly flatten its learning-curve. 

Class | Description
----- | -------------
connection | Represents an attachment to the middleware, as a network-addressable entity.  Encapsulates both the transport and payload bridge libraries.
session | Encapsulates a callback thread, and is implemented using mamaQueue and mamaDispatcher.
publisher | Thin wrapper over mamaPublisher
subscriber | Thin wrapper over mamaSubscription. Supports two flavors (regex and hierarchical) of wildcard subscriptions. 
subscriberEvents | Declares callback functions for subscription events.
timer | Encapsulates a mamaTimer object.
timerEvents | Declares callback functions for timer events.
request | Represents a mamaInbox, with associated requestEvents class for handling replies.
requestEvents | Declares callback functions for request events, i.e. replies directed to the request's underlying `mamaInbox`.
reply | The reply class simplifies sending responses to inbox requests.

The classes' relationships are illustrated below:<br>

![](ozimpl.png)

## Asynchronous callback events

Classes that generate asynchronous callbacks are associated with a session object, which provides the queue/dispatcher/thread on which the callbacks are invoked.  These classes also have an associated "xxxEvents" class that defines the callback methods for the appropriate source.  Applications can choose whether to define the events class separately, or to multiply-inherit from both the source and sink events.  (See the `req2.cpp` sample for an example of the latter approach).

The destructor for event sources is declared `protected` in order to prevent it being called from application code -- instead these classes define a `destroy` method that calls the appropriate MAMA function to tear down the event source.  The MAMA code enqueues the destroy on the object's queue -- when the object is eventually destroyed MAMA calls an `onDestroy` callback, which OZ then uses to do the final `delete this`.

## Compiler support
The `oz` classes target C++11 -- this allows the code to be cleaner and more readable, compared to the older C++98 standard used by OpenMAMA's native C++ support.  (We currently don't use features specific to C++14 and above in order to work with as wide a range of compilers as possible).

## Memory management/Smart pointers/RAII
To simplify memory management, all the `oz` objects *must* be allocated on the heap  -- it is not possible to allocate them on the stack.  (Constructors are all `protected` -- the factory function(s) are the only way to create objects).

In keeping with the recommendations in [EMC++](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/) and the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines), the factory functions in OZ return `unique_ptr`'s in most cases.  These have no space or speed penalties relative to raw pointers, and can be easily converted to `shared_ptr`'s if desired.   (The exception is the `oz::connection::getPublisher` method, which returns a `shared_ptr` to a publisher from the connection's collection).


In addition, each class has a custom deleter defined that respects the OZ convention of destroying objects by calling their `destroy` methods, rather than their destructors.

Since the smart pointer `deleter`'s are invoked in reverse order of their creation, this ensures a clean tear-down of the MAMA objects.

If an application prefers to manage object lifetimes itself, it can call the `unique_ptr::release` method to acquire a raw pointer, which it is then responsible for.  (See the `req.cpp` program for an example of this). Note that if you choose to manage object lifetimes manually, you should make sure to do so as documented in the [Developer Guide](http://www.openmama.org/sites/default/files/OpenMAMA%20Developer%27s%20Guide%20C.pdf>).


## Messages
Conspicuous perhaps by its absence is a wrapper class for `mamaMsg`.  The main reason for this is the sheer number of methods that would be required -- another reason is that the `mamaMsg` API is actually relatively easy to understand and use, compared to some of the other MAMA APIs.

This may come at some point, but is currently not a priority.

## Publishers
The connection classes maintains a collection of publishers, keyed by topic.  This is a potential performance optimization, since publishers can be shared among all threads that need to publish messages.

Where this really comes into play is when publising replies to inboxes -- since a single topic (publisher) is used for all inbox replies, there is no need to create a publisher for each inbox.

The publishers are reference-counted and deleted when no longer used.


## Macros
The OZ header defines two macros that can be helpful, `CALL_MAMA_FUNC` and `TRY_MAMA_FUNC`.

`CALL_MAMA_FUNC` calls a MAMA function -- if the called function returns anything other than `MAMA_STATUS_OK`, it logs a message before returning the mama_status.

`TRY_MAMA_FUNC` calls a MAMA function -- if the MAMA function returns anything other than `MAMA_STATUS_OK`, it logs a message and throws the mama_status.

The OZ library code uses `CALL_MAMA_FUNC` exclusively, while the sample programs use `TRY_MAMA_FUNC` to ensure that any errors cannot be ignored.

When using `TRY_MAMA_FUNC`, any errors will be displayed similar to:

```
8/12 10:28:54.776905|mama_loadBridgeWithPathInternal|31394-7f79dabdb7c0|ERR(0,0) mama_loadmamaPayload(): Could not open middleware bridge library [mamaxxximpl] [libmamaxxximpl.so: cannot open shared object file: No such file or directory]|mama.c(2662)
8/12 10:28:54.776988|start|31394-7f79dabdb7c0|ERR(0,0) Error 26(NO_BRIDGE_IMPL)|ozimpl.cpp(18)
8/12 10:28:54.777001|main|31394-7f79dabdb7c0|ERR(0,0) Error 26(NO_BRIDGE_IMPL)|pub.cpp(22)
terminate called after throwing an instance of 'mama_status'
Aborted (core dumped)
```

## Naming Conventions

The OZ classes use camelCase for pretty much everything, except macros which use ALL_CAPS_WITH_UNDERSCORES.

A 'p' prefix is used for raw pointers -- e.g., `subscriber* pSubscriber`.  That is the only time type information is encoded in a name.

Class member variables are named with a trailing underscore, e.g., `conn_`.



