# botuvera

A small HTTP/HTTPS static file server written in C++23. I use it to serve [viniciuscestari.dev](https://viniciuscestari.dev).

## Features

- Static file serving from an in-memory cache
- TLS support via OpenSSL
- Non-blocking epoll event loop with coroutines and HTTP keep-alive
- ETag revalidation and Cache-Control

## Dependencies

- C++23 compiler
- CMake >= 3.30
- OpenSSL

## Build

```sh
cmake -B build
cmake --build build
```

## Usage

```
usage: build/botuvera [options] <root-dir>
       build/botuvera -h | --help
       build/botuvera --version

Simple Web server.

  <root-dir>               directory to serve
  -p, --port <port>        port to listen on (default: 7432)
  -tls-p --tls-port <port> port to listen on TLS connection (default: 8432)
      --host <host>        host to bind to (default: 127.0.0.1)
      --max-age <seconds>  Cache-Control max-age for non-HTML files; 0 means no-cache (default: 0)
      --html-max-age <seconds> Cache-Control max-age for HTML files; 0 means no-cache (default: 0)
      --cert <path>        path to cert.pem (optional)
      --key <path>         path to key.pem (optional)
  -h, --help               show this help and exit
  --version                show version and exit
```

### Example

```sh
# HTTP only
./build/botuvera /var/www/html

# HTTP + HTTPS
./build/botuvera --cert cert.pem --key key.pem /var/www/html
```

## Why "botuvera"?

While navigating through Google Maps in Brazil, I randomly stumbled upon Botuverá, a small rural city in Brazil. I liked the name, so that's it.