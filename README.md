A tiny web server in c
======================

I am reading
[Computer Systems: A Programmer's Perspective](http://csapp.cs.cmu.edu/).
It teachers me how to write a tiny web server in C.

I have written another
[tiny web server](https://github.com/shenfeng/nio-httpserver) in JAVA.

### Features
1. Basic MIME mapping
2. Very basic directory listing
3. Low resource usage
4. [sendfile(2)](http://kernel.org/doc/man-pages/online/pages/man2/sendfile.2.html)
5. Support Accept-Ranges: bytes (for in browser MP4 playing)

### non-features
1. No security check
2. No concurrency

### Usage
Open a http server here
like python -m SimpleHTTPServer, for daily use,  but should be faster.

### TODO

1. write a epoll version for performance comparison with my JAVA version
