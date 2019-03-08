# Publish/Subscribe
The central paradigm for many distributed messaging applications is "publish/subscribe" messaging -- this is where messages are sent, not to individual destinations, but to "subjects" or "topics", which are typically arbitrary strings.

Applications receive these messages by "subscribing" to specific topics which they are interested in, and the middleware software is responsible for routing these messages over the physical transport(s) (e.g., TCP) to subscribers.

## Topics

There are basically two different approaches in wide use for defining topics: one is a hierarchical approach similar to that used by Tibco Rendezvous and AMQP, another approach is regular expressions.   

Unfortunately, ZeroMQ supports neither of these -- it only supports filtering on a message "prefix".  When an application subscribes to a filter pattern (using `zmq_setsockopt(..., ZMQ_SUBSCRIBE, ...)` the specified prefix is simply matched against incoming[^pubfilt] messages using e.g. `memcmp`.  

OZ builds on ZeroMQ's prefix matching to support full-featured topic matching, using regular expressions.  (We use regexes since they are a superset of hierarchical topics).

When an application subscribes to a topic with OZ, the code checks if the subscribed topic contains any "wildcard" characters.

- If not, then the subscribed topic *is* the prefix, and only regular ZeroMQ prefix matching is required.

- If the subscribed topic *does* contain any wildcard characters, OZ translates that wildcard subject into a prefix (the non-wildcarded portion of the subject), which is used to subscribe at the ZeroMQ level, and a regex which is used to further filter messages that pass ZeroMQ's prefix matching.

Obviously, the more selective the prefix filter, the better.  Subscribing to a wildcard subject like "*" is likely to be rather inefficient.

For this approach to work well, wildcard subjects should be constructed such that any constant portion is at the beginning, while wildcards themselves should be at the end.

[^pubfilt]:  In fact, recent versions of ZeroMQ perform message filtering on the *sending* (publish) side, not on the receiving side, for point-to-point protocols like TCP.  Message filtering is done on the receiving side only for multicast protocols like PGM.
