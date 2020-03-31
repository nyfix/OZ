
## ZeroMQ Socket Monitor
ZeroMQ is able to provide connect/disconnect (and other) events via its API (<http://api.zeromq.org/4-2:zmq-socket-monitor>).

In normal operation, these events largely duplicate information that is already available in discovery messages.  Nevertheless, these events can be useful for troubleshooting, so the code logs them at "reasonable" severity levels, depending on the event -- increase the logging level to see more detail. 

Currently the socket monitor is enabled by default -- it can be enabled by setting the following in mama.properties:

    mama.zmq.transport.<transport_name>.socket_monitor=0

At default logging level, the discovery messages for a typical process look something like the following (the `zmqBridgeMamaTransportImpl_monitorEvent ` messages):

~~~
11:52:31.479202|zmqBridgeMamaTransport_create:Initializing transport with name transact (transport.c:117)11:52:31.487575|zmqBridgeMamaTransportImpl_init:Bound publish socket to:tcp://127.0.0.1:44928  (transport.c:353)11:52:31.487816|zmqBridgeMamaTransportImpl_monitorEvent:socket:132fda0 name:dataSub value:31 event:1 desc:CONNECTED endpoint:tcp://127.0.0.1:44928 (transport.c:1613)11:52:31.487983|zmqBridgeMamaTransportImpl_monitorEvent:socket:132fda0 name:dataSub value:0 event:4096 desc:HANDSHAKE_SUCCEEDED endpoint:tcp://127.0.0.1:44928 (transport.c:1613)11:52:31.488849|zmqBridgeMamaTransportImpl_monitorEvent:socket:1338b40 name:namingPub value:33 event:1 desc:CONNECTED endpoint:tcp://127.0.0.1:5756 (transport.c:1613)11:52:31.489076|zmqBridgeMamaTransportImpl_monitorEvent:socket:1338b40 name:namingPub value:0 event:4096 desc:HANDSHAKE_SUCCEEDED endpoint:tcp://127.0.0.1:5756 (transport.c:1613)11:52:31.489974|zmqBridgeMamaTransportImpl_monitorEvent:socket:13418e0 name:namingSub value:34 event:1 desc:CONNECTED endpoint:tcp://127.0.0.1:5757 (transport.c:1613)11:52:31.490143|zmqBridgeMamaTransportImpl_monitorEvent:socket:13418e0 name:namingSub value:0 event:4096 desc:HANDSHAKE_SUCCEEDED endpoint:tcp://127.0.0.1:5757 (transport.c:1613)11:52:31.490939|zmqBridgeMamaTransportImpl_init:Connecting naming sockets to publisher:tcp://127.0.0.1:5757 subscriber:tcp://127.0.0.1:5756 (transport.c:367)11:52:31.491236|MamaAdapterTest|11365-140060968142592|INFO(0,0) **** Starting iteration #1|MamaAdapterTest.cpp(288)11:52:31.491239|zmqBridgeMamaTransportImpl_dispatchNamingMsg:Received naming msg: type=C prog=MamaAdapterTest host=bt uuid=f99a376f-f1b1-482c-807f-012ade397ac6 pid=11365 topic=_NAMING pub=tcp://127.0.0.1:44928 (transport.c:562)11:52:31.491272|zmqBridgeMamaTransportImpl_dispatchNamingMsg:Got own endpoint msg -- signaling event (transport.c:569)
~~~

If enabled, the socket monitor runs on a dedicated thread that *only* polls the monitor sockets.  

OZ does *not* use monitor events for any purpose internally.  Monitor events are only reported via the application log. 
