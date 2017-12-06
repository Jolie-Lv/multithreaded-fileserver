# CS 8803 PR3 Readme
William Emmanuel (wemmanuel3@gatech.edu)

## Project Description
### Proxy Server
For the proxy server, I used the standard easy_libcurl examples. I created two libcurl requests: one to find the size of the requested file, and another to actually download the data. When the size is found in the first request, it is relayed to the client with the GF_OK header. If the file is not found, an error is relayed instead. The libcurl data handler continually calls gfs_send as the data is received.

### Proxy Cache
My proxy cache can be explained with a diagram:
+-----------------------+                             +-----------------------+
|           Proxy       |                             |     Cache             |
|                       |                             |1. get proxy request   |
|1. set up queue of     +-------------------------->  |2. load as much of file|
|  shared mem segments  |                             |  into shared mem      |
|2. get client request  |  Request to cache sent      |  segment as possible  |
|3. dequeue shared mem  |  over message queue         |3. set relevant shm    |
|4. send cache request  |     (file, start_byte,      |  fields, like size_left
|5. initialize and wait |      shared_memory_id)      |4.post on shared mem   +
|  on semaphore in shm  |                             |  semaphore to notify  |
|6. send data to client |                             |  proxy                |
|7. requeue shared mem  |                             |                       |
|   segment             |  <--------------------------+                       |
|                       |    Data and other info      |                       |
|                       |    sent over shared memory  |                       |
|                       |   (control_semaphore,size,  |                       |
|                       |    size_left, file_data)    |                       |
+-----------------------+                             +-----------------------+

Here's a more detailed step-by-stem overview of how a request would be serviced:
Proxy:
1. Proxy receives request for file
2. Proxy dequeues one of it's pre-created shared memory segments
3. In this shared memory, the proxy sets up a semaphore
4. The proxy sends a request over a message queue to the cache that contains the file name, shared memory id, and start byte.
5. The proxy waits on set up semaphore
6. The proxy sends the data in shared memory to the client
7. The proxy checks if any more requests need to be made for more file data. It will repeat to step 4 if so

Cache:
1. Cache receives a request through a message queue
2. Cache attaches the shared memory sent in that request
3. Cache checks if it has the requested file. If not, it will set the shared memory size field to -1 and unlock the semaphore
4. Cache copies as much of the requested file as possible into the shared memory segment (starting at the start byte)
5. Cache sets the size of the data copied and size of file left in the shared memory segment
6. Cache unlocks the semaphore

## Known Issues
One proxy cache test is failing for 'multi-threading efficacy'. I think this is a side effect of my design choices. I decided to only relay requests between the proxy and cache with a max size of the specified segment size. The proxy has to make (file_size / segment_size) requests to the cache to get the full file. I think this is a positive design feature. It stops large file transfers from clogging up the cache process, and in reality happens very quickly since all requests and responses are over IPC. However, if the test measures multi-threading efficacy via slowing down the cache some how, my implementation would suffer.   

## References
* Used the [tldp.org System V IPC](http://www.tldp.org/LDP/lpg/node21.html) pages heavily for IPC help
* Used all unix man pages a lot
* Used [libcurl examples](htthttps://curl.haxx.se/libcurl/c/example.html/) for help with part 1
