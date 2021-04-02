# bsc-project-rdma
Bachelor project 2021


## Progress
### Todo
- allow server to handle multiple clients:
	- very naively polls on event channel now, i.e. its only polling for one connection and expects a connect only --> expand to handle disconnect
	- client buffers (fd)
	- for now: infinitely handle requests
- setup way to input k-v pairs: stdin(pipes/test)/file
- setup requests
- setup global way to add stuff to hash table
	- Put in util.c/.h

### Done
- Basic setup
- Add glib for hashmap
- setup basic rdma communication between server client

## Build
- `cmake .` for config
- `make` for compilation and linking

## Run
- `bin/pears_server [ARGS]` for running server
- `bin/pears_client [ARGS]` for running client

Or:
- `./run_server.sh` for example
- `./run_client.sh` for example
