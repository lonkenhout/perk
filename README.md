# bsc-project-rdma
Bachelor project 2021


## Progress
### Todo
- allow server to handle multiple clients:
	- very naively polls on event channel now, i.e. handles connect and disconnect, does so for multiple clients up to MAX\_CLIENTS
	- add polling on incoming client requests: completion channel fd?, poll on buffer?
	- for now: infinitely handle requests
- setup way to input k-v pairs: client can read from stdin or file, reads 10 pairs and then exits
- add extra buffer for server to write to
- let server actually store kv pairs
- setup global way to add stuff to hash table
	- Put in util.c/.h
- do some cleanup...

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
