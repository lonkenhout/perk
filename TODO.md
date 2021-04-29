## Progress
### Todo
- implement version using memcached
- implement versions using multiple writes/reads
- setup benchmarking system
- MORE CLEANUP..

### Done
- Basic setup
- Add glib for hashmap
- Setup basic rdma communication between server client
- Server stores kv pairs
- Server can handle connecting/disconnecting multiple clients dynamically
- Multithreaded
	- Workers use variety of methods to check for arrival of requests
	- Use variety of methods to send back results
- gen\_small\_workload.py for generating a workload of 1,000,000 requests
