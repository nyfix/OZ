OZ is configured using the "mama.properties" file, as described in the [OpenMAMA Developer's Guide](http://www.openmama.org/sites/default/files/OpenMAMA%20Developer%27s%20Guide%20C.pdf). 

In the following examples, settings are prefixed by "mama.zmq.transport.{name}.", where "{name}" is the name of the transport, as specified in `mamaTransport_create`.

## Common Settings

Parameter | Default Value | Description
-------- | -------- | ---------- 
type|tcp|OZ currently supports only tcp.  Any other value is silently ignored.
publish_address|lo|Specifies the interface the transport should use to publish messages.  Applies to both naming and data sockets.
socket_monitor|1|Specifies whether to enable monitoring of socket connects/disconnects.  When active, socket activity will be logged to `stderr`.  For more information, see <http://api.zeromq.org/master:zmq-socket-monitor>.
is_naming|1|Specifies that the transport is a "naming" transport.  For more information, see [Naming Service](Naming_Service).


### Naming Sockets
The following settings apply only to "naming" transports (i.e., transports that use an nsd/proxy connection to discover peers).

Parameter | Default Value | Description
-------- | -------- | ----------
naming.subscribe_address_0 | 127.0.0.1 | Specifies the address of an nsd/proxy process to connect to.
naming.subscribe_port_0 | 5756 | Specifies the port at which to connect to the nsd/proxy process specified in `subscribe_address_0`.
naming.subscribe_address_1, naming.subscribe_address_2 |  | Similar to `subscribe_address_0`, except no default value.
naming.subscribe_port_1, naming.subscribe_port_2 | | Similar to `subscribe_port_0 `, except no default value.
naming.wait_for_connect|1|Specifies that the transport should block until it receives at least one "welcome" message from nsd/proxy. The transport will make `connect_retries` attempts, waiting `connect_interval` seconds after each attempt.  <br>If the transport has still not received a welcome message, it will terminate with an error. 
naming.connect_retries|100||
naming.connect_interval|.1| 
naming.retry_connects|1|Whether to retry connects on the naming sockets. <br>Note that this does *not* apply to the initial connection (see `connect_retries` above for that), but rather in the case where an established nsd/proxy connection has been disconnected.  <br>This is implemented in the transport by calling  `zmq_setsockopt(..., ZMQ_RECONNECT_IVL)` with the value of `retry_interval`.
naming.retry_interval|10| 
naming.beacon_interval|1|Specifies how often to publish "beacon" (announcement) messages.  If set to zero, no beacons will be sent.  Cannot be less than .1 (100 ms).

### Data Sockets

The following settings apply to "data" sockets, in both naming and non-naming modes.

Parameter | Default Value | Description
-------- | -------- | ---------- 
retry_connects|1|Whether to retry connects on the data sockets.<br>This is implemented in the transport by calling  `zmq_setsockopt(..., ZMQ_RECONNECT_IVL)` with the value of `retry_interval`.
retry_interval|10| 

<br>
The following settings apply to data sockets, but only in non-naming mode.

Parameter | Default Value | Description
-------- | -------- | ---------- 
incoming_url, incoming_url_1 .. incoming_url_256||Specifies endpoint addresses that should be used for incoming data connections.  Whether to bind or connect is determined based on whether the url specifies a wildcard address (bind) or not (connect).  
outgoing_url, outoging_url_1 .. outoging_url_256||Specifies endpoint addresses that should be used for outgoing data connections.  Whether to bind or connect is determined based on whether the url specifies a wildcard address (bind) or not (connect).  

## Hard-coded Settings
The following socket options are hard-coded at present, and can not be changed.  They apply to all sockets opened by the transport.

Socket Option | Value | Description
-----| ---- | ----
ZMQ_RCVHWM | 0 | High-water mark for incoming messages, above which ZeroMQ will take corrective action.  A value of zero means no limit, and this is also the ZeroMQ default.
ZMQ_SNDHWM | 0 | High-water mark for outgoing messages, above which ZeroMQ will take corrective action.  A value of zero means no limit, and this is also the ZeroMQ default.
ZMQ_BACKLOG | 200 | Maximum number of pending connections.
ZMQ_LINGER | 0 | Specifies that any pending messages should be discarded when the socket is closed.  
ZMQ_IDENTITY | | The sockets' identity property is set to a string that can be useful when debugging (e.g., "dataPub").
 
 

