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

## Run
- `python[3] get_small_workload.py` for generating a small sample workload
- `bin/pears_server [ARGS]` for running server
- `bin/pears_client [ARGS]` for running client

Or:
- `./run_server.sh` for example
- `./run_client.sh` for example
