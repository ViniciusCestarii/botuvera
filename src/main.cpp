#include "config.hpp"
#include "connection.hpp"
#include "http.hpp"
#include "socket.hpp"
#include "static_server.hpp"
#include "tls.hpp"

#include <arpa/inet.h>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

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
    TCPSocket server;
    server.reuse_address();
    server.bind(cfg->host, cfg->port);
    server.listen();
    std::cout << "Serving " << cfg->root << " on " << cfg->host << ":"
              << cfg->port << "\n";

    auto handle = [&file_server](std::unique_ptr<Connection> conn,
                                 sockaddr_in client) {
      std::cout << "Connection from " << inet_ntoa(client.sin_addr) << ":"
                << ntohs(client.sin_port) << "\n";

      std::string buf;
      char chunk[1024];
      ssize_t n;

      while ((n = conn->recv(chunk, sizeof(chunk))) > 0) {
        buf.append(chunk, n);

        if (auto end = buf.find("\r\n\r\n"); end != std::string::npos) {
          HTTPRequest req(std::string_view(buf.data(), end + 4));
          std::cout << req;
          conn->send(file_server.serve(req).to_network_string());
          break;
        }
        if (buf.size() > 8192)
          break;
      }
    };

    if (tls_server) {
      std::thread([&] {
        TCPSocket tls_sock;
        tls_sock.reuse_address();
        tls_sock.bind(cfg->host, cfg->tls_port);
        tls_sock.listen();
        std::cout << "Serving TLS on " << cfg->host << ":" << cfg->tls_port
                  << "\n";

        while (true) {
          sockaddr_in client{};
          TCPSocket conn = tls_sock.accept(client);
          try {
            std::thread(handle,
                        std::make_unique<TLSConnection>(conn.release(),
                                                        tls_server->get_ctx()),
                        client)
                .detach();
          } catch (const std::exception &e) {
            std::cerr << "Failed to handle TLS handshake: " << e.what() << "\n";
          }
        }
      }).detach();
    }

    while (true) {
      sockaddr_in client{};
      TCPSocket conn = server.accept(client);
      try {
        std::thread(handle, std::make_unique<TCPSocket>(std::move(conn)),
                    client)
            .detach();
      } catch (const std::exception &e) {
        std::cerr << "Failed to handle connection: " << e.what() << "\n";
      }
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
  }
}
