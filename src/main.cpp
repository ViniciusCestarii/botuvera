#include "config.hpp"
#include "http.hpp"
#include "socket.hpp"
#include "static_server.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char **argv) {
  auto cfg = parse_config(argc, argv);

  if (!cfg)
    return 1;

  StaticFileServer file_server(cfg->root);

  try {
    TCPSocket server;
    server.reuse_address();
    server.bind(cfg->host, cfg->port);
    server.listen();
    std::cout << "Serving " << cfg->root << " on " << cfg->host << ":" << cfg->port << "\n";

    while (true) {
      sockaddr_in client{};
      TCPSocket conn = server.accept(client);

      std::cout << "Connection from " << inet_ntoa(client.sin_addr) << ":"
                << ntohs(client.sin_port) << "\n";

      std::string buf;
      char chunk[1024];
      ssize_t n;

      while ((n = conn.recv(chunk, sizeof(chunk))) > 0) {
        buf.append(chunk, n);

        if (auto end = buf.find("\r\n\r\n"); end != std::string::npos) {
          HTTPRequest req(std::string_view(buf.data(), end + 4));
          std::cout << req;
          conn.send(file_server.serve(req).to_network_string());
          break;
        }
        if (buf.size() > 8192)
          break;
      }
    }
  } catch (const std::system_error &e) {
    std::cerr << e.what() << "\n";
  }
}
