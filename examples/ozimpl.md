# OZ classes
The `oz` namespace defines several classes designed to provide a simple, easy-to-use introduction to OpenMAMA and OZ.

Class | Description
----- | -------------
connection | Encapsulates both the transport and payload bridge libraries.
session | Encapsulates a callback thread, and is implemented using mamaQueue and mamaDispatcher.
publisher | Thin wrapper over mamaPublisher
subscriber | Thin wrapper over mamaSubscription.  Supports only basic (not market-data) subscriptions.
timer |
inbox |

The classes' relationships are illustrated below:<br>

![](ozimpl.png)

All the above classes have the following characteristics:

- The constructor is protected and so cannot be called from application code -- instead a (static) `create` method is provided that functions as the constructor.
  - Because of this, `oz` objects cannot be allocated on the stack -- they can only be allocated on the heap.
- The destructor is also protected and cannot be called from application code, instead a (virtual) `destroy` function is provided.
  - For event sinks (subscriber, timer and inbox) the actual destructor is called in response to an `onDestroy` callback.  This ensures that the `oz` object is only destructed when it is safe to do so.


## Compiler support
The `oz` classes target C++11 -- this allows the code to be cleaner and more readable, compared to the older C++98 standard used by OpenMAMA's native C++ support.



## Issues

### Safe delete

- make dtor protected
- provide create/destroy functions
- `destroy()` function for event sinks:
  - calls `mamaXXX_destroy` on mama object
  - `onDestroyCB` actually calls `delete`
- for non-event sinks, `destroy()` just calls `delete this`

### Separation of source vs. sink
It would be nice to be able to separate out the source class (e.g., `subscriber`) from the sink class (e.g., `subscriberEvents`).  

This is easily done with timers and inboxes, since each of these classes provides a separate closure argument for the normal event (e.g., `onTimer`) and the destroy callback.

That's not possible with subscriptions, however: there is a single closure specified for both `onMsg` etc. and the `onDestroy` callbacks.

It is simple enough to provide a separate callback in `oz` class, but that requires a double indirection in the `onMsg` callback.  That's clearly not ideal, but for now that's the approach we've taken.  ("First make it work, then make it fast").


