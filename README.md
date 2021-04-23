# bsc-project-rdma
Bachelor project 2021


## Progress
### Todo
- fix bugs:
	- sometimes hangs on connect.. dont know why..
- replace all regular poll with epoll()
- allow server to handle multiple clients:
	- very naively polls on event channel now, i.e. handles connect and disconnect, does so for multiple clients up to MAX\_CLIENTS
	- now polls on WRITEable memory associated with client (kinda spins on it tho..)
	- for now: infinitely handle requests
- gen\_small\_workload.py for generating a workload of 100,000 requests
- DO SOME CLEANUP..

### Done
- Basic setup
- Add glib for hashmap
- Setup basic rdma communication between server client
- Server stores kv pairs

## Build
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
- `python[3] get_small_workload.py` for generating a small sample workload
- `bin/pears_server [ARGS]` for running server
- `bin/pears_client [ARGS]` for running client

Or:
- `./run_server.sh` for example
- `./run_client.sh` for example
