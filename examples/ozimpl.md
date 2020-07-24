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

All the above classes have the following characteristics:

- The constructor is protected and so cannot be called from application code -- instead a (static) `create` method is provided that functions as the constructor.
  - Because of this, `oz` objects cannot be allocated on the stack -- they can only be allocated on the heap.  
- The destructor is also protected and cannot be called from application code, instead a (virtual) `destroy` function is provided.  
  - For event sinks (subscriber, timer and inbox) the actual destructor is called in response to an `onDestroy` callback.  This ensures that the `oz` object is only destructed when it is safe to do so.

  
## Compiler support
The `oz` classes target C++11 -- this allows the code to be cleaner and more readable, compared to the older C++98 standard used by OpenMAMA's native C++ support.


  
