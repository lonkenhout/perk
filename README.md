# bsc-project-rdma
Bachelor project 2021


## Build
### Dependencies
- Install `memcached`, `libmemcached`
- `cmake .` for config
- `make` for compilation and linking

## Run on DAS5
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

## Run
- `python[3] gen_small_workload.py` for generating a small sample workload
- `bin/pears_server [ARGS]` for running server
- `bin/pears_client [ARGS]` for running client

Or you can use the run scripts, which supply the executables with a number of default arguments.
- `./run_server.sh`, try `./run_server.sh -h` for options
- `./run_client.sh`, try `./run_client.sh -h` for options

Currently, the default options are 5,000,000 requests, using RDMA write/send setup.
