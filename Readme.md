# CS 8803 PR1 Readme
William Emmanuel (wemmanuel3@gatech.edu)

## Project Description

### Echo
The echo warmup was implemented in a straightforward manner. Both the client and server start by configuring a `sockadder_in` struct. On the server side, a socket is then set up. The server enters a continues loop where:

* a connection is accepted
* a 16 byte buffer is declared and cleared
* 16 bytes are read from the socket into that buffer
* those same 16 bytes are sent back over the socket
* the socket is closed.

On the client side, first 16 bytes are read from the terminal. A socket is then opened with the server, and those 16 bytes are sent. 16 bytes are then read back from the socket and printed to the console.
I found the most difficult part of this exercise was getting up to speed on programming in C and sockets. There are no known limitations of this echoclient and echoserver. All tests are passing.

### Transfer
For the transfer portion of the assignment, all socket configuration was identical to the echoserver and client. On the server side, a continuous routine is entered where the server:

* accepts an incoming connection
* opens a file specified in the command line
* finds the size of that file
* calls the sendfile() syscall continually, until the bytes transferred equal the calculated size
* close the connection

On the client side, a socket is configured and a connection is made to the server. A new file is opened to store the transferred data. recv() is then called continuously until the server stops sending data. Whenever recv() is called, data is read into a buffer, then written to the new file using fwrite. When the server is done sending data, the file and socket are closed.

There are no known limitations for this implementation. All tests are passing.

### gfserver
My gfserver implementation largely follows the layout in `gfserver.h`. First I created the `gfserver_t` struct. This contains a max pending variable, the server `sockaddr_in` struct, a handler, an argument, and a `running` flag. In `gfserver_create`, this struct is configured. I also created a `gfcontext_t` struct, which contains the client socket descriptor for each request.

The bulk of this `gfserver` logic is contained in the `gfserver_serve` function. This is a continuously running function that accepts connections, stores those connection sockets in a `gfcontext` object, and calls `recv` on that socket. The data from the socket is read into a buffer, and this string is passed to a function called `getPathFromRequest`. This function uses the `strtok` call to parse the request. It looks for GETFILE, followed by GET, a path name, and an end header marker. This path is then handed off to the gfserver handler. The handler calls `gfs_sendheader` and `gfs_send` with raw file data, and the connection is closed once the transfer is complete.

There are no limitations or failing test cases for this implementation.

### gfclient
My gfclient implementation follows the provided `gfclient.h` file closely. I first created the `gfcrequest_t` struct, which contains the server information (`sockaddr_in` struct, host, port number, socket), the call back handlers and handler arguments, the path, the status, the file size, and the number of bytes received. `gfc_create` creates this struct. The main logic of the client is contained in `gfc_perform`. This function does additional setup, and actually connects to the server via a socket. It then sends the file request (e.g. "GETFILE GET /path\r\n\r\n"). After that, it calls a function called `recvStatusAndSize`. This will call `recv` on the server socket until a full header is received. `parseStatusAndSize` will then parse this string using `strtok`. It will set the status and expected size on the request object. After that, the client will continually call `recv` until the expected file size is reached. For ever `recv` call, the data handler will be called with the data received.

There is one failing test with my gfclient implementation, discussed in the "Limitations" section below.

### Multithreaded gfserver
For the multithreaded gfserver portion of this assignment, I first created a pthread array. These are the request worker threads. I also created a queue of "thread work" items, each item containing a gfcontext, a path, and an argument. A mutex was created to protect this shared queue. When a request comes in, the boss thread acquires the work queue lock. A thread work object is created with the request, and the object is enqueued on the shared work queue. The lock is then removed, and all worker threads are notified with a "work available" condition variable. Each individual thread will wait for that broadcast, lock the work queue, pop a work item if available, and fulfill the request. There are no known limitations of this implementation, and all Bonnie tests are passing.

### Multithreaded gfclient
The multithreaded gfclient has a very similar layout to my multithreaded gfserver. First, all the files to request are translated to `gfcrequest_t` objects. All of these requests are put in a shared work queue. Then, an array of pthreads is initialized. Each of these threads run a routine where they acquire a lock to the shared work queue, pop a request object, and perform the actual file download. This routine will run until the work queue is empty, or the thread request limit is hit (specified in the command line). There are no known limitations of this implementation, and all Bonnie tests are passing.

## Known Issues
One gfclient test is failing. I could never replicate the problem locally, but a test exercising "the client properly handles an OK response and a short message (less than 1000 bytes)" continually failed on Bonnie. It appears that the file transfer size is incorrect on certain small transfers. I tried many things to try to replicate: I divided the header and data up into many send() calls, I sent both header and data in one send() call, and I tried both short binary and text files. Nothing seemed to fail on my machine. I have a hunch this is due to an abuse of the strlen() call, but could not find a good replacement.

## References
* Used the [Posix Threads Progamming](https://computing.llnl.gov/tutorials/pthreads/#ConVarSignal) page heavily for the multithreaded portion of the assignment
* Used all unix man pages a lot
* Used [this slideshow](http://www.csd.uoc.gr/~hy556/material/tutorials/cs556-3rd-tutorial.pdf) to help understand sockets
