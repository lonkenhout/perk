# PERK: distributed key-value store using RDMA
This repo contains the implementation of a distributed key-value store using RDMA.
Various types of communication primitives are tested in different configurations. 
By configuration, I mean the manner in which requests and responses are handled, currently the following have been implemented:

| Request \(C\) | Request \(S\) | Response \(C\) | Response \(S\) |
|-------------|------------|--------------|--------------|
| SEND      | - | - | SEND  |
| WRITE     | - | - | SEND  |
| WRITE     | - | - | WRITE |
| WRITE IMM | - | - | SEND  |
| WRITE     | - | READ | -  |

Here, the \(X\) indicates whether the action required for request/response is performed by the Client or Server.

## Quick refs
- Great RDMA example to get started with understanding RDMA implementations: [https://github.com/animeshtrivedi/rdma-example]()
- How to setup softiwarp if you do not have hardware support for RDMA:
	- [https://github.com/animeshtrivedi/blog/blob/master/post/2019-06-26-siw.md](), or
	- [https://www.reflectionsofthevoid.com/2020/07/software-rdma-revisited-setting-up.html]()
- How to RDMA? [https://www.rdmamojo.com/]()

## Implementation details
Unwritten stuff

## Build
### Dependencies
- GLIB2
#### Install
- Memcached: [https://memcached.org/downloads]()
For local installation: `./configure --prefix=/home/$USER/local && make && make install`
- Libmemcached: [https://launchpad.net/libmemcached/+download]()

#### Configuration
`cmake .` for config
`make` for compilation and linking

For extra functionality you can pass in temporary environment variables, currently, there are multiple for benchmarking purposes:
- `PERK_DEBUG=1`, turns on debugging prints
- `PERK_BM_LATENCY=1`, turns on latency macros (prints latency client side)
- `PERK_BM_OPS_PER_SEC=1`, turns on ops/sec macros (prints ops per sec client and server side)
- `PERK_BM_SERVER_EXIT=1`, server exits after all clients disconnect

How to pass them?
`PERK_BM_OPS_PER_SEC=1 PERK_BM_SERVER_EXIT=1 cmake .
make`


## Run
- `python[3] gen_small_workload.py` for generating a small sample workload
- `bin/pears_server [ARGS]` for running server (-h for help)
- `bin/pears_client [ARGS]` for running client (-h for help)

Or you can use the run scripts, which supply the executables with a number of default arguments.
- `./run_server.sh`, try `./run_server.sh -h` for options
- `./run_client.sh`, try `./run_client.sh -h` for options

Currently, the default options are 5,000,000 requests, using RDMA write/send setup.

## Run on DAS5
This section only applies if you have access to one of the DAS5 cluster computers:
- `module load cmake/3.15.4; module load gcc/9.3.0; module load prun`, to make sure the right modules are loaded:
- `preserve -np 2 -t 900`, to reserve some nodes

### Server side
1. `ssh node0..`, to connect to node
2. `cd bsc-project-rdma`, to cd to folder with runscript
3. `./run_server.sh`, to run server (no need to add IP, is now extracted from ifconfig), or<br />
   `./run_server.sh P`, to run server on port P

### Client side
1. `ssh node0..`, to connect to node
2. `cd bsc-project-rdma`, to cd to folder with runscript
3. `./run_client.sh X`, to make client connect to node X (it just substitutes it in the IP address), or <br />
   `./run_client.sh X P`, to make client connect to node X on port P

If you want to set the number of requests done, modify `count=5000000` to some other number.
This number of requests takes roughly 40 seconds.
