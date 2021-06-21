# PERK: distributed key-value store using RDMA
This repo contains the implementation of a distributed key-value store using RDMA.
Various types of communication primitives are tested in different configurations. 
By configuration, I mean the manner in which requests and responses are handled, currently the following have been implemented (or rather, allowed to be run):

| Request \(C\) | Response \(C\) | Request \(S\) | Response \(S\) |
|-------------|------------|--------------|--------------|
| SEND      | RECV | RECV    | SEND  |
| WRITE     | RECV | -    | SEND  |
| WRITE IMM | RECV | RECV | SEND  |
| WRITE     | -    | -    | WRITE |
| WRITE     | READ | -    | -  |

Here, the \(X\) indicates whether the action required for request/response is performed by the Client or Server.

## Quick refs
- Great RDMA example to get started with understanding RDMA implementations: [https://github.com/animeshtrivedi/rdma-example]()
- How to setup softiwarp if you do not have hardware support for RDMA:
	- [https://github.com/animeshtrivedi/blog/blob/master/post/2019-06-26-siw.md](), or
	- [https://www.reflectionsofthevoid.com/2020/07/software-rdma-revisited-setting-up.html]()
- How to RDMA? [https://www.rdmamojo.com/]()


## Build
### Dependencies
#### Install
- Memcached: [https://memcached.org/downloads]()
- Libmemcached: [https://launchpad.net/libmemcached/+download]()

For installing locally (without sudo), choose installation location:

`./configure --prefix=/home/$USER/local && make && make install`

#### Configuration
Cmake expects either a globally installed installation (found through regular system variables), or if you have trouble, you can manually set an environment variable: `LIBMEMCACHED_PATH=/home/$USER/libmemcached-1.0.18`, e.g. if you have the source code in your home dir. 
Then for actually generating build files and compiling:

`cmake .` for config
`[ENV] cmake .` for advanced config, see below

`make` for compilation and linking

#### Extra functionality through [ENV]
For extra functionality you can pass in temporary environment variables, currently, there are multiple for benchmarking purposes:
- `PERK_DEBUG=1`, turns on debugging prints
- `PERK_BM_LATENCY=1`, turns on latency macros (prints latency client side)
- `PERK_BM_OPS_PER_SEC=1`, turns on ops/sec macros (prints ops per sec client and server side)
- `PERK_BM_SERVER_EXIT=1`, server exits after all clients disconnect
- `PERK_PRINT_REQUESTS=1`, turns on request printing client-side
- `PERK_OVERRIDE_VALSIZE=<VAL>`, overrides the maximum value size of PERK (utility for benchmark setups)

How to pass these variables?

`PERK_BM_OPS_PER_SEC=1 PERK_BM_SERVER_EXIT=1 cmake .`
`make`


## Run
### Workloads
- For generating a sample workload for 1 or multiple clients:
`python3 gen_small_workload.py <REQUESTS> <GET_DISTRIBUTION> <MAX_CLIENTS> <OUTPUT_DIR>`

e.g.:
`python3 gen_small_workload.py 1000000 0.95 16 /var/scratch/$USER/input`, this generates 16 1,000,000-request files with a GET/SET ratio of 95:5 and stores them in the folder /var/scratch/$USER/input.

### Actually running
Running the server:
`./bin/perk_server [ARGS]`

Running the client:
`./bin/perk_client [ARGS]`



Or you can use the run scripts, which supply the executables with a number of default arguments.
- `./run_server.sh`, try `./run_server.sh -h` for options
- `./run_client.sh`, try `./run_client.sh -h` for options

Currently, the default options are 3,000,000 requests, using RDMA write/send setup.

## Run on DAS5
This section only applies if you have access to one of the DAS5 clusters:
- `module load cmake/3.15.4; module load gcc/9.3.0; module load prun`, to make sure the right modules are loaded:
- `preserve -np 2 -t 900`, to reserve some nodes

### Server side
1. `ssh node0..`, to connect to node
2. `cd bsc-project-rdma`, to cd to folder with runscript
3. `./run_server.sh -n -r <comp>`, to run server (-n binds to local node, where it expects a `ib0` infiniband interface), check `./run_server.sh -h` for example comps.

### Client side
1. `ssh node0..`, to connect to node
2. `cd bsc-project-rdma`, to cd to folder with runscript
3. `./run_client.sh -n <N> -r <comp>`, where N is the node number (substitutes in a default ib address), check `./run_client.sh -h` for example comps and other options.

If you want to set the number of requests done, modify `count=5000000` to some other number.
This number of requests takes roughly 40 seconds.
