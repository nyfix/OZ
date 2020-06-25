# Reconnections and Heartbeats

OZ was designed to run over WAN links that can prove unreliable, and takes advantage of ZeroMQ's [automatic reconnection feature](https://www.google.com/search?q=ZMQ_RECONNECT_IVL) to make that work.  

> Note that there is no "disconnect" message in the ZMTP protocol 
 (disconnects are detected at the TCP level by the receipt of a `RST` or `FIN` packet), so there is no way for ZeroMQ to distinguish between a "normal" and "abnormal" disconnect (e.g., process termination or crash).  The OZ naming protocol *does* provide for that distinction with a specific "disconnect" message.
 
ZeroMQ's automatic reconnection works reasonably well for processes that bind to "well-known" ports, but it works less well for processes that bind to "wildcard" ports, which are satisfied by the OS choosing an available ephemeral port.

The basic problem with "vanilla" reconnects in ZeroMQ is that once a connection is disconnected, ZeroMQ will try ***forever*** to reconnect to that endpoint.  With processes configured to listen at well-known ports, that's not a problem -- it's a feature.  But with ephemeral ports that approach doesn't work so well.  Since OZ is designed to use ephemeral ports pretty much everywhere, that presented a problem:

- The most common problem scenario is when a process terminates.  If the process terminates gracefully, it is likely that its peers will receive an OZ "disconnect" message (see [Naming Service](Naming-Service.md), which will disconnect the ZeroMQ socket and remove the endpoint from OZ's internal table of connected peers.  (Note that OZ doesn't rely on the disconnect message for correct operation -- it just makes things cleaner).

- However, if the process crashes it is likely that the disconnect message will not be sent.  This [PR](https://github.com/zeromq/libzmq/pull/3831) solves that problem for the case where the reconnect immediately returns ECONNREFUSED -- i.e., where there is no process listening at the original port.

Being ~~paranoid~~ risk-averse, we've also submitted a [PR](https://github.com/zeromq/libzmq/pull/3960) that closes a couple of additional loopholes related to connecting to non-ZeroMQ processes:

- Adds an option to stop reconnecting if a peer fails the ZTMP handshake.
  - Failure of the handshake was not being detected/reported properly in all cases, so the PR also includes a fix for that.
- Previously, a peer that accepted a connection and then immediately closed it would be be retried, potenially forever.  With this patch, a peer that immediately closes the connection is treated like a failed handshake.

## Heartbeats
In some cases, network outages present themselves as simply a lack of data.  To detect those problems, OZ enables heartbeats on the ZeroMQ client (connecting) sockets. The `heartbeat_interval ` can be specified in `mama.properties` (default is 10 seconds).

## OZ configuration

Heartbeats and reconnects are enabled on sockets that call `zmq_connect` -- i,e, the namingSub and dataSub sockets.  They are also enabled on the namingPub socket, since that socket connects to the nsd (at the address in the welcome message).

The option to stop reconnecting on either ECONNREFUSED or failure of the ZMQ handshake is set on the dataSub socket, and also on the namingPub socket.  (The namingPub socket connects to the endpoint specified in the welcome message from the nsd).  

- It is *NOT* specified on the namingSub socket -- since that socket connects to a well-known endpoint, we want reconnects to be retried "forever".

> Note: The default value of ZMQ_RECONNECT_IVL is 100ms.  For OZ we felt a better choice is 10 seconds -- at least for our use case, any network "glitches", especially on WAN links, tend to take a minimum of some number of seconds to resolve.  The `reconnect_interval ` setting in `mama.properties` can be used to change the default value.

## Reuse of ephemeral ports
Since ephemeral ports are assigned by the OS, and since there are a finite number of them, they will eventually be reused and assigned to new processes.  Related to this is that both ZeroMQ and OZ will ignore duplicate attempts to connect to the same endpoint.  

So it's at least theoretically possible that a new OZ process could end up with the same endpoint address as an earlier process that terminated.  This is not a problem because:

- With the changes discussed above, ZeroMQ will stop reconnecting on failure, and remove the failed endpoint from its internal table.  Subsequent attempts to connect to the same endpoint will be treated as new requests, not as duplicates.

- Even if OZ doesn't receive a disconnect message from a terminated process, the socket will still be disconnected at the ZeroMQ level.  And, since OZ uses a UUID to identify connections, there is no possibility that a subsequent connection attempt will be ignored as a duplicate.

