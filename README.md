# botuvera

A small HTTP/HTTPS static file server written in C++23. I use it to serve [viniciuscestari.dev](https://viniciuscestari.dev).

## Features

- Static file serving
- TLS support via OpenSSL
- One OS thread per connection

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
./build/botuvera [options] <root-dir>

  <root-dir>               directory to serve
  -p, --port <port>        port to listen on (default: 7432)
  -p-tls, --port-tls <port> TLS port (default: 8432)
  --host <host>            host to bind to (default: 127.0.0.1)
  --cert <path>            path to cert.pem
  --key  <path>            path to key.pem
  -h, --help               show help and exit
  --version                show version and exit
```

### Example

```sh
# HTTP only
./build/botuvera /var/www/html

# HTTP + HTTPS
./build/botuvera --cert cert.pem --key key.pem /var/www/html
```
