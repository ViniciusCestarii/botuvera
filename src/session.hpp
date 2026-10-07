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
#include <netinet/in.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <utility>

inline void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

struct LoopRegistration {
  int fd;
  ~LoopRegistration() { EventLoop::instance().remove(fd); }
};

template <class Conn> Task serve_connection(Conn conn, StaticFileServer &fs) {
  int fd = conn.fd();
  LoopRegistration registration{fd};

  std::string buf;
  char chunk[1024];
  bool close_conn = false;

  auto &loop = EventLoop::instance();
  loop.set_deadline(fd);

  while (!close_conn) {
    if (auto end = buf.find("\r\n\r\n"); end != std::string::npos) {
      std::optional<HTTPRequest> req;
      bool bad_request = false;
      try {
        req.emplace(std::string_view(buf.data(), end + 4));
      } catch (const std::exception &) {
        bad_request = true;
      }

      size_t total_len = bad_request ? end + 4 : end + 4 + req->get_content_length();

      if (bad_request || buf.size() >= total_len) {
        HTTPResponse resp;
        bool keep_alive = false;
        if (bad_request) {
          resp.set_status(HTTPStatus::BadRequest);
        } else {
          keep_alive = req->wants_keep_alive();
          resp = fs.serve(*req);
          if (req->get_version() != HTTPVersion::UNKNOWN)
            resp.set_version(req->get_version());
        }
        resp.set_header("Connection", keep_alive ? "keep-alive" : "close");
        auto resp_str = resp.to_network_string();
        const char *p = resp_str.data();
        size_t remaining = resp_str.size();
        bool send_err = false;
        while (remaining > 0) {
          loop.set_deadline(fd);
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
        if (send_err || !keep_alive) {
          close_conn = true;
        } else {
          buf = buf.substr(total_len);
          loop.set_deadline(fd);
        }
        continue;
      }

      if (total_len > 8192) {
        close_conn = true;
        continue;
      }
    } else if (buf.size() > 8192) {
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
}

// TLS entry point: completes the handshake, then hands the connection off to
// the shared serve_connection loop. The handshake is the only TLS-specific
// step, so it lives here rather than leaking into the generic loop.
inline Task serve_tls_connection(int fd, SSL_CTX *ctx, StaticFileServer &fs) {
  TLSConnection conn(fd, ctx);
  EventLoop::instance().set_deadline(fd);

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
      // The listener fd itself is broken; nothing further can be accepted.
      if (errno == EBADF || errno == EINVAL || errno == ENOTSOCK)
        break;
      // Everything else (EAGAIN, fd exhaustion, a client that reset before
      // accept() completed, a signal interrupt, ...) is transient: wait for
      // the listener to be readable again and retry instead of shutting the
      // acceptor down permanently.
      co_await ReadReady{server_fd};
      continue;
    }
    set_nonblocking(client_fd);
    try {
      start(client_fd);
    } catch (const std::exception &) {
      // Drop this connection but keep accepting.
    }
  }
}
