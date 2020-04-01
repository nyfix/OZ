
## ZeroMQ Socket Monitor
ZeroMQ is able to provide connect/disconnect (and other) events via its API (<http://api.zeromq.org/4-2:zmq-socket-monitor>).

In normal operation, these events largely duplicate information that is already available in discovery messages.  Nevertheless, these events can be useful for troubleshooting, so the code logs them at "reasonable" severity levels, depending on the event -- increase the logging level to see more detail. 

Currently the socket monitor is enabled by default -- it can be enabled by setting the following in mama.properties:

    mama.zmq.transport.<transport_name>.socket_monitor=0

At default logging level, the discovery messages for a typical process look something like the following (the `zmqBridgeMamaTransportImpl_monitorEvent ` messages):

~~~
11:52:31.479202|zmqBridgeMamaTransport_create:Initializing transport with name transact (transport.c:117)
~~~

If enabled, the socket monitor runs on a dedicated thread that *only* polls the monitor sockets.  

OZ does *not* use monitor events for any purpose internally.  Monitor events are only reported via the application log. 