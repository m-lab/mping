mping
=====

From http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.44.5928:

The Internet is suffering from multiple effects of its own rapid growth. Network providers are in the uncomfortable position of deploying new products and technologies into already congested environments without adequate tools to easily assess their performance. In this paper we present a diagnostic that provides direct measurement of IP performance, including queue dynamics at or beyond the onset of congestion. It uses a transport style sliding window algorithm combined with either ping or traceroute to sustain packet queues in the network. It can directly measure such parameters as throughput, packet loss rates and queue size as functions of packet and window sizes. Other parameters, such as switching time per packet or per byte, can also be derived. Many of the measurements can be performed either in a test bed environment (yielding the most accurate results), on single routers in situ in the Internet, or along specific paths in the production Internet.

git
===

```
$ git pull
$ git submodule update --init --recursive
```

Make
=====
```
$ cmake .
$ make
```

Test sending and receiving 
=====
1. Un-comment include/mp_mping.h `#define MP_PRINT_TIMELINE`
2. make
The program will print out sending and receiving sequence at the end.
Note that under this mode, in every second, the program will only send 50 packets.
