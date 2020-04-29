# Performance

## Performance Testing
OpenMAMA includes rudimentary performance-testing programs, as documented at <https://openmama.github.io/openmama_performance_testing.html>.

### Testing with qpid

```
source qpid-pubsub.sh
./producer.sh -s AAAA
```

```
source qpid-pubsub.sh
./consumer.sh -s AAAA
                 TIME     RATE  TRANS LOW  TRANS AVG TRANS HIGH     GAPS  MC GAPS MISSING FIDS 
2020/04/21 - 12:05:33    32567         33         44       2431        0        0            0
2020/04/21 - 12:05:34    30156         33         90       9343        0        0            0
2020/04/21 - 12:05:35    30153         33         53       2535        0        0            0
2020/04/21 - 12:05:36    22954         35        147       9998        0        0            0
2020/04/21 - 12:05:37    30570         34         63       4309        0        0            0
2020/04/21 - 12:05:38    22973         34        121      10818        0        0            0
2020/04/21 - 12:05:39    32475         33         46       2779        0        0            0
2020/04/21 - 12:05:40    33118         33         44       2351        0        0            0
2020/04/21 - 12:05:41    28501         32         52       2481        0        0            0
...
```

> NOTE that with the native OpenMAMA qpid transport, the producer **MUST** be started before the consumer -- if the consumer is started first, it will not receive any messages.  This is a limitation of the qpid transport, and does not apply to OZ -- with OZ the producer and consumer can be started in any order.

### Testing with OZ

```
source oz-nsd.sh
./nsd.sh
```

```
source oz-nsd.sh
source omnmnsg.sh
./producer.sh -s AAAA
```

```
source oz-nsd.sh
source omnmnsg.sh
./consumer.sh -s AAAA
                 TIME     RATE  TRANS LOW  TRANS AVG TRANS HIGH     GAPS  MC GAPS MISSING FIDS 
2020/04/21 - 12:06:19   847552     708087     780015     838537        0        0            0
2020/04/21 - 12:06:20   666727     751804     845333     938246        0        0            0
2020/04/21 - 12:06:21   573908     938249    1104023    1413273        0        0            0
2020/04/21 - 12:06:22   446988    1413278    1700890    1900920        0        0            0
2020/04/21 - 12:06:23   540979    1779656    1916152    2173290        0        0            0
2020/04/21 - 12:06:24   623274    2173295    2279334    2354764        0        0            0
2020/04/21 - 12:06:25   660382    2189472    2265658    2371685        0        0            0
2020/04/21 - 12:06:26   774446    2366038    2489770    2629785        0        0            0
2020/04/21 - 12:06:27   746745    2629786    2775362    2826271        0        0            0
2020/04/21 - 12:06:28   715015    2742846    2759463    2775087        0        0            0
...
```

### Results
The OpenMAMA producer/consumer test measures message *rates* -- the producer sends as quickly as possible.

In this test, the qpid transport can acheive a rate of approximately 31,000 messages per second, while OZ with ZeroMQ manages around 650,000 messages per second, or about 20x the rate with qpid.

> Note that these results are **without** pinning either process to a CPU, which confers an advantage on OZ -- ZeroMQ is better able to take advantage of multiple cores, while qpid is essentially single-threaded on both the producer and consumer. 

The above results were obtained with:

- 4 x Intel(R) Core(TM) i7-4770R CPU @ 3.20GHz (with hyperthreading)
- 16GB RAM
- CentOS 7.7

The tests were run with qpid in point-to-point ("p2p") mode -- i.e., without a broker.  Performance with a brokered configuration, which is typical for qpid, would likely be less.

#### See also
[Qpid Bridge](https://openmama.github.io/openmama_qpid_bridge.html)

