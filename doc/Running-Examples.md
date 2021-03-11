## Running OpenMAMA examples

### Publish/Subscribe

First we start the nsd:

```
cd test
source setenv.sh
source oz-nsd.sh
./nsd.sh
3/31 15:18:20.417985|main|4086-7fbcb718c7c0|INFO(0,0) nsd running at tcp://127.0.0.1:5756|nsd.c(109)
```

After starting the nsd (see above), open another terminal window for the publisher:

```
cd test
source setenv.sh
source oz-nsd.sh
./pub.sh
...
Created inbound subscription.
3/31 15:36:35.451520|publishMessage|7776-7f2394270780|INFO(0,0) Publishing message 1 to MAMA_TOPIC {{MdMsgType[1]=1,MdMsgStatus[2]=0,MdSeqNum[10]=1,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)
...
3/31 15:36:37.453664|publishMessage|7776-7f2394270780|INFO(0,0) Publishing message 5 to MAMA_TOPIC {{MdMsgType[1]=0,MdMsgStatus[2]=0,MdSeqNum[10]=5,MdFeedHost[12]=MAMA_TOPIC}}|mamapublisherc.c(341)...

```

Then open another terminal window and start the subscriber -- as soon as you start the subscriber you should start seeing messages from the publisher:

```
cd test
source setenv.sh
source oz-nsd.sh
./sub.sh
...
mamasubscriberc: Created inbound subscription.
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                   27
              MdFeedHost    12               STRING           MAMA_TOPIC
...
mamasubscriberc: Recieved msg.
               MdMsgType     1                  I32                    0
             MdMsgStatus     2                  I32                    0
                MdSeqNum    10                  I32                   31
              MdFeedHost    12               STRING           MAMA_TOPIC
```

> NOTE that with the native OpenMAMA qpid transport, the publisher **MUST** be started before the subscriber -- if the subscriber is started first, it will not receive any messages.  This is a limitation of the qpid transport, and does not apply to OZ -- with OZ the publisher and subscriber can be started in any order.

#### See also
[Quick Start with OpenMAMA Example Apps](https://openmama.finos.org/openmama_quick_start_guide_running_openmama_apps.html)