#include "config.hpp"
#include "event_loop.hpp"
#include "session.hpp"
#include "socket.hpp"
#include "static_server.hpp"
#include "tls.hpp"

#include <exception>
#include <iostream>
#include <optional>

int main(int argc, char **argv) {
  auto cfg = parse_config(argc, argv);
  if (!cfg)
    return 1;

  StaticFileServer file_server(cfg->root, cfg->max_age, cfg->html_max_age);

  std::optional<TLSServer> tls_server;
  if (cfg->cert_path && cfg->key_path) {
    tls_server.emplace();
    tls_server->configure_ctx(*cfg->cert_path, *cfg->key_path);
  }

  try {
    auto &loop = EventLoop::instance();

    TCPSocket listener;
    listener.reuse_address();
    listener.bind(cfg->host, cfg->port);
    listener.listen();
    listener.set_nonblocking();
    std::cout << "Serving " << cfg->root << " on " << cfg->host << ":"
              << cfg->port << "\n";

    accept_loop(listener.fd(), [&](int fd) {
      serve_connection(TCPSocket(fd), file_server);
    });

    std::optional<TCPSocket> tls_listener;
    if (tls_server) {
      tls_listener.emplace();
      tls_listener->reuse_address();
      tls_listener->bind(cfg->host, cfg->tls_port);
      tls_listener->listen();
      tls_listener->set_nonblocking();
      std::cout << "Serving TLS on " << cfg->host << ":" << cfg->tls_port
                << "\n";

      accept_loop(tls_listener->fd(), [&](int fd) {
        serve_tls_connection(fd, tls_server->get_ctx(), file_server);
      });
    }

    loop.run();

  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
  }
}
