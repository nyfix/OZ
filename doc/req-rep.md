## Request/Reply Messaging
Request/reply messaging refers to the ability of an application to send a message which expects one or more replies, and to have those reply messages automatically routed to the request originator.  

While ZeroMQ includes its own version of request/reply messaging (ZMQ_REQ & ZMQ_REP socket types), its use [has been rather fragile in practice](https://stackoverflow.com/questions/26915347/zeromq-reset-req-rep-socket-state) -- specifically, ZeroMQ assumes a rigid ordering of requests and replies that is not flexible enough for many distributed applications.<sup>[1](#footnote1)</sup>

Instead, the OZ bridge implements request/reply "from scratch" using the same PUB/SUB sockets used for other traffic:

- At startup, the transport object generates a UUID that is used as the destination for all inbox replies.
- Individual request messages generate a unique 64-bit sequence number.
- The combination of the two is sufficient to uniquely identify an individual request, e.g., "_INBOX.d4ac532a-224f-11e8-a178-082e5f19101.0000000000000003".

Note that in order to guarantee the uniqueness of UUIDs, OZ uses the `uuid_generate_time_safe` function -- this means that the uuidd daemon must be installed and running on the host.  
 
<hr>

<a name="footnote1">1</a>: Newer versions of ZeroMQ provide a `ZMQ_REQ_RELAXED` socket option that is supposed to address this issue.  We decided not to pursue that option, at least in part to avoid a proliferation of sockets. 

