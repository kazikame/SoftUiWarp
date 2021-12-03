# SoftUiWARP
SoftUiWARP is a userspace library implementation of the iWARP protocol in software, that can
inter-operate with existing iWARP stacks. This repository includes an example of this usage in
`fake_ping_client.cpp`, which inter-operates with the `rping` utility. Additionally, this repository
includes performance benchmarks, measuring RDMA Read/Write latency/bandwidth.

## Building
To build the SoftUiWARP library and the benchmarks, run the following commands.
```bash
git clone https://github.com/kazikame/SoftUiWarp
cd SoftUiWarp
mkdir build ; cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Run SoftUiWARP perftest benchmarks

If you want to run a perftest benchmark, say `read_lat`, you need to run the binary on a client and
a server.

E.g. on the server, run;
```bash
./perftest/read_lat -s <server-ip>
```

And on the client, run:
```bash
./perftest/read_lat -s <server-ip> -c
```

## Run rping client

Right now, to inter-operate with the existing SoftiWARP stack, you need to enable the `RPING`
definition in `common/common.h`, and re-build the project. Once you've done that, you can run the
rping client like so:

1. Run rping on a server with an iWARP interface. Details on setting this up can be found 
[here](https://gist.github.com/ksonbol/d09727079501832b4a9608eaee42f4e9).
```bash
rping -d -s -a 10.10.1.2 -p 8888
```

2. Run `fake_ping_client` on another server
```bash
./fake_ping_client 10.10.1.2 8888
```

The rping server should output `ESTABLISHED`, and then proceed to ping back and forth.

