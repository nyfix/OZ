# Introduction
OZ provides a commercial-quality open-source transport bridge implementation for OpenMAMA using ZeroMQ middleware.  

It supports wildcard topics, request/reply (inbox) messaging, a dynamic peer discovery protocol, and thread-safety.

# Wire format
OZ works with any payload bridge supported by OpenMAMA, but is typically used with the ["Omnium" payload bridge](https://github.com/cascadium/OpenMAMA-omnm), and that is the default payload library used if none is specified.

## Application messages
OZ uses the following wire-format for all application messages (messages sent using `mamaPublisher_send`):

```
         +--------------------+
         |                    |
         |     subject        |
         |     (1-256)        |
         +--------------------+
         |     null (1)       |
         +--------------------+
         |   msg type (1)     |
         +--------------------+
         |                    |
         |    reply addr      |
         |  (60, optional)    |
         +--------------------+
         |      null (1)      |
         +--------------------+
         |                    |
         |                    |
         |     payload        |
         |                    |
         |                    |
         +--------------------+
```

- subject - message topic, variable-length, delimited by null
- msg type - message type code, currently used values are:
 - 0x1: normal pub/sub message
 - 0x2: inbox request
 - 0x3: inbox reply
- reply addr - if the messsage is an inbox request, the reply address follows.  It is exactly 60 bytes.
- payload - the serialized buffer obtained from the payload bridge by calling `mamaMsg_getByteBuffer`   

## Naming messages
Naming messages are exchanged by peers via the nsd proxy (see [Naming Service](nsd.md) for more information):

```
          +--------------------+
          |                    |
          |     subject        |
          |      (256)         |
          +--------------------+
          |     null (1)       |
          +--------------------+
          |   msg type (1)     |
          +--------------------+
          |                    |
          |   program name     |
          |      (256)         |
          +--------------------+
          |     null (1)       |
          +--------------------+
          |                    |
          |    host name       |
          |      (256)         |
          +--------------------+
          |     null (1)       |
          +--------------------+
          |     pid (4)        |
          +--------------------+
          |                    |
          |  transport uuid    |
          |      (36)          |
          +--------------------+
          |      null (1)      |
          +--------------------+
          |                    |
          |   endpoint addr    |
          |      (256)         |
          +--------------------+
          |      null (1)      |
          +--------------------+
```

- subject - message topic ("_NAMING")
- msg type - naming message type, currently used values are:
 - "C": connect message
 - "c": connect beacon message
 - "D": disconnect message
 - "W": welcome message from proxy (see <https://somdoron.com/2015/09/reliable-pubsub/> for more info)
- program name - retrieved from `program_invocation_short_name`
- host name - from `gethostname`
- pid - process ID
- transport uuid - unique ID of the transport
- endpoint addr - the endpoint address of the transport's PUB socket, established by `zmq_bind`.  This is the address that peers' SUB sockets specify in `zmq_connect` call.
  