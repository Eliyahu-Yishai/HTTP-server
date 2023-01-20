Authored by Eliyahu Yishai

# HTTP-server

==Description==
This server handle the connections with the clients. 
the server creates a socket for each client it talks to. 
In other words, there is always one socket where the server listens to 
connections and for each client connection request, the server opens another socket. 
In order to enable a multithreaded program, the server create threads that handle
the connections with the clients. Since the server should maintain a limited number of threads, 
it constructs a thread pool. In other words, the server creates the pool of threads in
advance, and each time it needs a thread to handle a client
connection, it enqueues the request so an available thread in the pool will handle it.

==files==
threadpool.c, threadpool.h 
Threads will be built according to the amount requested from it, and each client request will receive its own thread

server.c 
A server that receives http requests from the client, and writes back to the socket what is required of it


==how to compaile?== 
gcc -pthread threadpool.c server.c -o server
./server <port> <pool-size> <max-number-of-request>

==after compile==
after the compile the server Will wait for requests.
how to request files from server?
-1 method-
You need to open terminal inside the dir of the project,
and type:
telnet localhost <port>
GET </path> HTTP/1.0

-2 method-
You need to open browser, and type:
localhost:<port></path>

==Output== 
In one method, you will see the characters of the file,
including the header of the file.
In a second method, you will see the presentation of the file,
in a visual form of video/image/text
