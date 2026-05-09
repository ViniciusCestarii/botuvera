#include "http.hpp"
#include "socket.hpp"
#include "static_server.hpp"

#include <arpa/inet.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

constexpr uint16_t PORT = 3000;

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <root-dir>\n";
    return 1;
  }

  std::error_code ec;
  fs::path root = fs::canonical(argv[1], ec);
  if (ec || !fs::is_directory(root)) {
    std::cerr << "invalid root directory: " << argv[1] << "\n";
    return 1;
  }

  StaticFileServer file_server(root);

  try {
    TCPSocket server;
    server.reuse_address();
    server.bind(PORT);
    server.listen();
    std::cout << "Serving " << root << " on port " << PORT << "\n";

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
