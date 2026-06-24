#pragma once

#include "connection.hpp"
#include "event_loop.hpp"
#include "http.hpp"
#include "static_server.hpp"
#include "tls.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <utility>

inline void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

template <class Conn> Task serve_connection(Conn conn, StaticFileServer &fs) {
  int fd = conn.fd();

  std::string buf;
  char chunk[1024];
  bool close_conn = false;

  while (!close_conn) {
    if (auto end = buf.find("\r\n\r\n"); end != std::string::npos) {
      HTTPRequest req(std::string_view(buf.data(), end + 4));
      std::cout << req;
      bool keep_alive = req.wants_keep_alive();
      auto resp = fs.serve(req);
      if (req.get_version() != HTTPVersion::UNKNOWN)
        resp.set_version(req.get_version());
      resp.set_header("Connection", keep_alive ? "keep-alive" : "close");
      auto resp_str = resp.to_network_string();
      const char *p = resp_str.data();
      size_t remaining = resp_str.size();
      bool send_err = false;
      while (remaining > 0) {
        auto r = conn.poll_send(p, remaining);
        if (r == IOResult::WantWrite)
          co_await WriteReady{fd};
        else if (r == IOResult::WantRead)
          co_await ReadReady{fd};
        else if (r == IOResult::Error) {
          send_err = true;
          break;
        }
      }
      if (send_err || !keep_alive)
        close_conn = true;
      else
        buf = buf.substr(end + 4);
      continue;
    }

    if (buf.size() > 8192) {
      close_conn = true;
      continue;
    }

    ssize_t n = 0;
    auto r = conn.poll_recv(chunk, sizeof(chunk), n);
    if (r == IOResult::Done && n > 0)
      buf.append(chunk, n);
    else if (r == IOResult::WantRead)
      co_await ReadReady{fd};
    else if (r == IOResult::WantWrite)
      co_await WriteReady{fd};
    else
      close_conn = true;
  }

  EventLoop::instance().remove(fd);
}

// TLS entry point: completes the handshake, then hands the connection off to
// the shared serve_connection loop. The handshake is the only TLS-specific
// step, so it lives here rather than leaking into the generic loop.
inline Task serve_tls_connection(int fd, SSL_CTX *ctx, StaticFileServer &fs) {
  TLSConnection conn(fd, ctx);

  for (auto r = conn.poll_handshake(); r != IOResult::Done;
       r = conn.poll_handshake()) {
    if (r == IOResult::WantRead)
      co_await ReadReady{fd};
    else if (r == IOResult::WantWrite)
      co_await WriteReady{fd};
    else {
      EventLoop::instance().remove(fd);
      co_return;
    }
  }

  serve_connection(std::move(conn), fs);
}

// Accepts connections on server_fd forever, handing each accepted fd to start()
// which is expected to spawn a serve_connection coroutine for it.
template <class Start> Task accept_loop(int server_fd, Start start) {
  while (true) {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    int client_fd =
        ::accept(server_fd, reinterpret_cast<sockaddr *>(&client), &len);
    if (client_fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        co_await ReadReady{server_fd};
        continue;
      }
      std::cerr << "accept: " << strerror(errno) << "\n";
      break;
    }
    set_nonblocking(client_fd);
    std::cout << "Connection from " << inet_ntoa(client.sin_addr) << ":"
              << ntohs(client.sin_port) << "\n";
    start(client_fd);
  }
}
