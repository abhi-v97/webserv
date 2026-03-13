*This project has been created as part of the 42 curriculum by avalsang, atyurina, and evmouka.*

# Webserv

## Description

A fast, responsive web server built using C++ 98 as a learning exercise to better understand HTTP and Ngnix. It can handle multiple concurrent connections, serve static files of any type, handle CGI requests for dynamic content, manage range requests for audio and video streams, and manage file upload/deletion.

## Technical Details

### I/O Multiplexing

A good web server is fast and responds quickly. Since requests can come in at any time in any number, a web server must be able to handle them simutaneously. If the web server uses a naive approach and simply handles one request at a time from start to finish, the server would block until a request is completely over and there would be major gaps between each request since I/O operations tend to be slower than other computations.

To achieve a fast and efficient response, the server manages concurrent requests using a method called **I/O multiplexing**, where multiple requests are handled through one single event loop. Instead of blocking the CPU while waiting for I/O operations (reading a large file, waiting for a file upload), the program keeps track of multiple requests and processes each one as soon as they're ready.

This event-driven approach is preferable to other means of handling multiple jobs at once like multithreading and multiprocessing due to its scalability and lower resource overhead. If you create multiple threads to handle requests, one thread that's doing heavier computation can slow down other threads since they share the same memory space and resources. If you employ multiprocessing, where each request is its own process, they no longer slow each other down, but now you need more overhead since each process needs its own memory and inter-process communication is slow.

**I/O multiplexing** instead uses an event-driven approach, where the program monitors each request and works on them when they are ready and skips over them when they need to wait. This requires careful state management and isn't ideal for CPU heavy tasks, but is able to handle parallel requests efficiently.

The multiplexing was implmenented using the **poll()** function. This was chosen over other options like select and epoll due to its simplicity and efficiency when handling a smaller set of file descriptors. 

### Design Principles

The web server was implemented using the [reactor design pattern](https://en.wikipedia.org/wiki/Reactor_pattern). The reactor pattern is designed to be used with multiplexing to achieve high throughput and scalability and is a good starting point for implementing a web server. It couples each request with a separate handler which uses callbacks to the event handler for better [separation of concerns](https://en.wikipedia.org/wiki/Separation_of_concerns). This allows for great modularity as new handlers can be added without modifying the main event loop. Apart from web servers, it can also be used for database access, messaging systems, or inter-process communications where concurrent event-handling problems need to be resolved.

In terms of downsides, the reactor pattern relies heavily on [callbacks](https://en.wikipedia.org/wiki/Callback_(computer_programming)), which makes analysing and debugging more challenging. Single thread can also be a drawback though it can be overcome by using multithreading in conjuction with the reactor pattern. 

Our implementation of this pattern involves the following mechanisms:

- **Handle**: an interface to a specific request. In the case of our web server, it means a network socket or a pipe file descriptor.
- **Dispatcher**: the event loop of the web server, its responsible for registering valid event handlers and invoking them when an event is raised.
- **Listener**: this handler is tied to the socket where listen requests will occur, asking our web server to respond to a new incoming connection. 
- **ClientHandler**: the request handler responsible for a single connection, it is responsible for judging the state of a current request and processing it appropriately.
- **CgiHandler**: called by ClientHandler when applicable, it manages the inner child process of the invoked CGI script and is responsible for feeding it relevant data or reading its output and processing it before its sent back to the client.
- **IHandler**: an abstract interface class. Each new handler must implement this interface so that Dispatcher can operate through it. 

### Request Handling

HTTP request data can come in varying chunks. You could even get mutliple requests in one single read event. To combat this, the request parser was built using the state-machine design principle. It waits until it has a full line to process and consumes those bytes from the buffer. Then, the line is interpreted and the state is changed appropriately. This allows the parser to handle as many requests as there are in the buffer, or deal with requests that are fed to it one byte at a time. 

An HTTP request has the following format:

#### Request line

```
METHOD URI HTTP-VERSION (request-line)
key: value (header fields)

message body (optional)
```

The first line is the request line. It includes:
- Method: e.g. GET, POST, PUT, DELETE
- Request Target: the path or endpoint (e.g. /api/users)
- HTTP Version: usually HTTP/1.1 or HTTP/2

#### Header fields

Next the heder fields carry metadata about the request. Example:

```
Host: api.example.com
User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:125.0) Gecko/20100101 Firefox/125.0
Content-Type: application/json
Connection: keep-alive
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8
Accept-Language: en-US,en;q=0.5
Authorization: Bearer <token>
```

#### Request body

The body contains data sent to the server, typically with PUT or POST requests. It can be in the form of JSON data, form data, or raw text. The format is specified by the `Content-Type` header field.

## Instructions

The web server needs a config file to be set up first. The design of the config file mimicks Nginx configs. Here's a basic example:

```
server {
    listen 8080;
    root www/html;
	server_name example.com;
	error_page 404 /errors/404.html;

	# catch all
    location / {
		root www/html;
        autoindex off;
		index index.html;
        allow_methods GET;
    }
}
```

Here's a list of features that are supported:

- server{}: use to create separate virtual hosts, allowing one web server to serve different websites. 
- location{}: use to delineate locations and set different rules for each.
- cgi{}: use to set up cgi rules inside a location block.
- listen: address:port or port.
- server_name: domain name.
- root: document root.
- alias: path translation.
- index: default file to serve when path points to the directory.
- allow_methods: valid methods for this location.
- client_max_body_size: limit request body size.
- autoindex: enable generating a directory index (on/off).
- error_page: custom error pages.

Run with make:

```
make [config.conf]
```

then test with browser URL: http://address:port, eg http://localhost:8080.

Or make a manual request with curl:

```
curl -i http://localhost:8080/index.html
```

Or use netcat:

```
netcat -v -C localhost 8080
```

### Resouces

- [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html): HTTP/1.1 RFC 
- [Doxygen Commands](https://www.doxygen.nl/manual/commands.html): Used for documentation
- [HTTP Status Codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status): HTTP Response Codes
- [Nginx docs](https://nginx.org/en/docs/): Nginx documentation, used for config files
- [Reactor Pattern](https://www.dre.vanderbilt.edu/~schmidt/PDF/reactor-siemens.pdf): Paper on reactor pattern
- [RFC 3875](https://datatracker.ietf.org/doc/html/rfc3875): CGI RFC