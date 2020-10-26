# Welcome to OZ!

## What is OZ?
OZ is a production-quality, high-performance implementation of ["message-oriented middleware"](https://en.wikipedia.org/wiki/Message-oriented_middleware) that is ready for use by applications in many areas, including but not limited to financial market data applications.

OZ was designed to support many of the most popular features of typical MOM's:

- publish/subscribe messaging using topic-based addressing, supporting hierarchical topic namespaces and "wildcard" subscriptions;
- request/reply (inbox) messaging for transactional interactions;
- dynamic service discovery with minimal configuration;
- broker-less architecture for reduced latency and optimum throughput;
- self-describing messages (thanks to [OpenMAMA-omnm](https://github.com/cascadium/OpenMAMA-omnm.git)).


## Who uses it?

OZ is the middleware layer that powers the [NYFIX Marketplace](https://www.itiviti.com/nyfix) -- the world's largest [FIX](https://en.wikipedia.org/wiki/Financial_Information_eXchange) network, which processes on the order of 50 million messages per day with unmatched reliability and outstanding performance.

## What's so special about OZ?
OZ is the only production-quality, high-performance middleware solution that is also open-source.

While there have been plenty of open-source middleware solutions (like [JMS](https://en.wikipedia.org/wiki/Java_Message_Service) and [AMQP](https://en.wikipedia.org/wiki/Apache_ActiveMQ)), none of them have really been suitable for the demands of real-time, low-latency, high-throughput messaging.

There *are* a number of high-performance middleware solutions, but they are almost exclusively commercial products.

## Why is it called OZ?
OZ brings together two world-class software projects: [OpenMAMA](https://openmama.org) and [ZeroMQ](https://zeromq.org/):

- OpenMAMA provides a ["middleware-agnostic messaging API"](https://www.openmama.org/what-openmama) that has been used for many years in a number of [commercial products and in-house systems](https://www.openmama.org/what-is-openmama/the-openmama-organisation/steering-committee).  OpenMAMA grew out of the original MAMA implementation by Wombat Financial, and [was donated](https://www.linuxfoundation.org/press-release/2011/10/financial-services-industry-leads-collaboration-on-new-openmama-project/) to the Linux Foundation by the New York Stock Exchange.  OpenMAMA also provides utility functions including timers, queues, dispatchers, loggers, etc.

    However, the [Qpid](https://openmama.github.io/openmama_qpid_bridge.html) open-source transport supplied with OpenMAMA has not been a match for the features, reliability or performance of the commercial OpenMAMA transports, and is not typically suitable for production use.

- ZeroMQ has been quite succesful as a high-performance messaging library, and is included in many Linux distributions (including Debian, Ubuntu, Fedora, CentOS, RHEL, and SUSE).  It is also used internally by a number of organizations including AT&T, Cisco, EA, Los Alamos Labs, NASA, Zynga, Spotify, Samsung Electronics, Microsoft, and CERN.

    However, ZeroMQ is essentially a "toolkit" that can be used to build applications, and to some extent that is both a blessing and a curse.  A blessing in that ZeroMQ doesn't impose an architecture on the applications you can build with it, but a curse in that this design approach means that there can be a rather steep learning curve, as well as a fair amount of effort, associated with building applications using ZeroMQ.  

OZ aims to combine the ease of building applications written to the OpenMAMA API with the commercial-quality performance and robustness of the ZeroMQ messaging library.

## What do I need to run it?
Currently, OZ runs on Linux (CentOS 6 and above).  We hope to provide support for other distributions and other platforms soon.

OZ runs over standard TCP/IP networks. 

## How do I get started?
The [Getting Started](doc/Getting-Started.md) document explains how to build OZ, and its dependencies, from source and how to run the OpenMAMA unit tests.  Once you've done that, OZ includes [example code](examples/Readme.md) that you can use to get familiar with OZ.

And/or check out the rest of the docs in the [doc](doc/README.md) directory.

## Acknowledgements 
OZ is based on work originally done by Frank Quinn, who implemented an earlier version of an OpenMAMA-ZeroMQ bridge, which can be found [here](https://github.com/cascadium/OpenMAMA-zmq). 

OZ has benefited greatly from the efforts of the entire ZeroMQ community, and especially Doron Somech, Luca Boccassi and Simon Giesecke.

OZ has benefited greatly from the efforts of the entire OpenMAMA community, and especially Frank Quinn.
