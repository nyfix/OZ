# OZ - OpenMAMA/ZeroMQ Middleware Bridge

This project is a "bridge" (library) that implements the [OpenMAMA](openmama.org) API  using the [ZeroMQ](zeromq.org) messaging library.

This is literally a "marriage made in heaven" in that OpenMAMA and ZeroMQ complement each other incredibly well.  

- OpenMAMA defines an API [^mama] that is well-suited for high-throughput, low-latency messaging applications. From the [OpenMAMA website](http://www.openmama.org/what-openmama): 

    > 	OpenMAMA is an open source project that provides a high performance middleware agnostic messaging API that interfaces with a variety of message oriented middleware systems

    The [OpenMAMA API](https://openmama.github.io/reference/mama/c/) was originally designed for financial market data, and includes support for publish/subscribe messaging using self-describing messages, along with other features (like initial images) that are important for market-data applications.  
    
    The OpenMAMA API continues to be used for that purpose by a number of vendors of commercial middleware, including Vela Trading, Solace, Tick42 and others. These vendors implement the OpenMAMA API on top of their proprietary messaging software.

    Unfortunately, when NYSE donated OpenMAMA to the Linux Foundation in 2011 the only open-source bridge library available was the [AVIS bridge](https://github.com/OpenMAMA/OpenMAMA-avis), which was not usable for anything other than toy applications. 
    
    That situation improved somewhat with the implementation of an OpenMAMA bridge library for the Apache Qpid middleware.  However, while the Qpid implementation was a far sight better than the earlier AVIS bridge, it was still not up to the quality of the commercial offerings.
    
- ZeroMQ has been quite succesful as a high-performance messaging library, and is included in many Linux distributions (including Debian, Ubuntu, Fedora, CentOS, RHEL, and SUSE).  It is also being used by a number of organizations including AT&T, Cisco, EA, Los Alamos Labs, NASA, Weta Digital, Zynga, Spotify, Samsung Electronics, Microsoft, and CERN.[^0mqrefs]

    However, ZeroMQ is essentially a "toolkit" that can be used to build applications, and to some extent that is both a blessing and a curse.  A blessing in that ZeroMQ doesn't impose an architecture on the applications you can build with it, but a curse in that this design approach means that there can be a rather steep learning curve, as well as a fair amount of effort, associated with building applications using ZeroMQ.  
    
This project aims to combine the ease of building applications written to the OpenMAMA API with the commercial-quality performance and robustness of the ZeroMQ messaging library.

## Acknowledgements 
This project is based on work originally done by Frank Quinn, who implemented an earlier version of an OpenMAMA-ZeroMQ bridge, which can be found [here](https://github.com/cascadium/OpenMAMA-zmq). 

This project has benefited greatly from the efforts of the entire ZeroMQ community, and especially Doron Somnech, Luca Boccassi and Simon Giesecke.

This project has benefited greatly from the efforts of the entire OpenMAMA community, and especially Frank Quinn.

## Caveats
Both OpenMAMA and ZeroMQ support a number of different platforms, including various flavors of Linux, and Windows.  Currently, this project is only supported on RedHat/CentOS Linux version 6 and above.

This project is built against a forked version of OpenMAMA version 6.2.1, and includes several important changes and improvements.  These changes have not yet been merged into the main OpenMAMA code base.

ZeroMQ can be built to take advantage of newer language features (e.g., C++11 support), but this project only supports gnu89/gnu++98 standards, and because of that ZeroMQ (and OpenMAMA) are also forced to build against those standards.

We are hopeful that, with help from the OpenMAMA and ZeroMQ communities, these limitations can be resolved in the near future.

[^mama]: MAMA, for Middleware Agnostic Messaging API
[^0mqrefs]: <http://zeromq.org/intro:read-the-manual>

