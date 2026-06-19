#include "config.hpp"
#include "event_loop.hpp"
#include "http.hpp"
#include "socket.hpp"
#include "static_server.hpp"
#include "tls.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <string_view>
#include <unistd.h>

static void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv) {
  auto cfg = parse_config(argc, argv);
  if (!cfg)
    return 1;

  StaticFileServer file_server(cfg->root);

  std::optional<TLSServer> tls_server;
  if (cfg->cert_path && cfg->key_path) {
    tls_server.emplace();
    tls_server->configure_ctx(*cfg->cert_path, *cfg->key_path);
  }

  try {
    auto &loop = EventLoop::instance();

    TCPSocket server;
    server.reuse_address();
    server.bind(cfg->host, cfg->port);
    server.listen();
    server.set_nonblocking();
    std::cout << "Serving " << cfg->root << " on " << cfg->host << ":"
              << cfg->port << "\n";

    auto handle = [&](int fd, sockaddr_in client) -> Task {
      std::cout << "Connection from " << inet_ntoa(client.sin_addr) << ":"
                << ntohs(client.sin_port) << "\n";

      std::string buf;
      char chunk[1024];
      bool close_conn = false;

      while (!close_conn) {
        if (auto end = buf.find("\r\n\r\n"); end != std::string::npos) {
          HTTPRequest req(std::string_view(buf.data(), end + 4));
          std::cout << req;
          bool keep_alive = req.wants_keep_alive();
          auto resp = file_server.serve(req);
          if (req.get_version() != HTTPVersion::UNKNOWN)
            resp.set_version(req.get_version());
          resp.set_header("Connection", keep_alive ? "keep-alive" : "close");
          auto resp_str = resp.to_network_string();
          const char *p = resp_str.data();
          size_t remaining = resp_str.size();
          bool send_err = false;
          while (remaining > 0 && !send_err) {
            ssize_t w = ::send(fd, p, remaining, 0);
            if (w > 0) {
              p += w;
              remaining -= w;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
              co_await WriteReady{fd};
            } else {
              send_err = true;
            }
          }
          if (send_err || !keep_alive) {
            close_conn = true;
          } else {
            buf = buf.substr(end + 4);
          }
          continue;
        }

        if (buf.size() > 8192) {
          close_conn = true;
          continue;
        }

        ssize_t n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n > 0) {
          buf.append(chunk, n);
        } else if (n == 0) {
          close_conn = true;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
          co_await ReadReady{fd};
        } else {
          close_conn = true;
        }
      }

      loop.remove(fd);
      ::close(fd);
    };

    auto accept_loop = [&](int server_fd) -> Task {
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
        handle(client_fd, client);
      }
    };

    accept_loop(server.fd());

    std::optional<TCPSocket> tls_sock;

    if (tls_server) {
      tls_sock.emplace();
      tls_sock->reuse_address();
      tls_sock->bind(cfg->host, cfg->tls_port);
      tls_sock->listen();
      tls_sock->set_nonblocking();
      std::cout << "Serving TLS on " << cfg->host << ":" << cfg->tls_port
                << "\n";

      auto tls_handle = [&](int fd, sockaddr_in client) -> Task {
        TLSConnection conn(fd, tls_server->get_ctx());

        for (auto r = conn.poll_handshake(); r != TLSPoll::Done;
             r = conn.poll_handshake()) {
          if (r == TLSPoll::WantRead)
            co_await ReadReady{fd};
          else if (r == TLSPoll::WantWrite)
            co_await WriteReady{fd};
          else {
            loop.remove(fd);
            co_return;
          }
        }

        std::cout << "TLS connection from " << inet_ntoa(client.sin_addr)
                  << ":" << ntohs(client.sin_port) << "\n";

        std::string buf;
        char chunk[1024];
        bool close_conn = false;

        while (!close_conn) {
          if (auto end = buf.find("\r\n\r\n"); end != std::string::npos) {
            HTTPRequest req(std::string_view(buf.data(), end + 4));
            std::cout << req;
            bool keep_alive = req.wants_keep_alive();
            auto resp = file_server.serve(req);
            if (req.get_version() != HTTPVersion::UNKNOWN)
              resp.set_version(req.get_version());
            resp.set_header("Connection", keep_alive ? "keep-alive" : "close");
            auto resp_str = resp.to_network_string();
            const char *p = resp_str.data();
            size_t remaining = resp_str.size();
            bool send_err = false;
            while (remaining > 0 && !send_err) {
              auto wr = conn.poll_send(p, remaining);
              if (wr == TLSPoll::WantWrite)
                co_await WriteReady{fd};
              else if (wr == TLSPoll::WantRead)
                co_await ReadReady{fd};
              else if (wr == TLSPoll::Error)
                send_err = true;
            }
            if (send_err || !keep_alive) {
              close_conn = true;
            } else {
              buf = buf.substr(end + 4);
            }
            continue;
          }

          if (buf.size() > 8192) {
            close_conn = true;
            continue;
          }

          ssize_t n;
          auto r = conn.poll_recv(chunk, sizeof(chunk), n);
          if (r == TLSPoll::Done && n > 0) {
            buf.append(chunk, n);
          } else if (r == TLSPoll::WantRead) {
            co_await ReadReady{fd};
          } else if (r == TLSPoll::WantWrite) {
            co_await WriteReady{fd};
          } else {
            close_conn = true;
          }
        }

        loop.remove(fd);
      };

      auto tls_accept_loop = [&](int server_fd) -> Task {
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
            std::cerr << "tls accept: " << strerror(errno) << "\n";
            break;
          }
          set_nonblocking(client_fd);
          tls_handle(client_fd, client);
        }
      };

      tls_accept_loop(tls_sock->fd());
    }

    loop.run();

  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
  }
}
