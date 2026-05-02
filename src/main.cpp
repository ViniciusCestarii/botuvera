#include "socket.hpp"
#include "utils.hpp"

#include <arpa/inet.h>
#include <iostream>

constexpr uint16_t PORT = 3000;

int main() {
  try {
    TCPSocket server;

    server.reuse_address();
    server.bind(PORT);
    server.listen();
    std::cout << "Listening on port " << PORT << "\n";

    while (true) {
      sockaddr_in client{};
      TCPSocket conn = server.accept(client);

      std::cout << "Connection from " << inet_ntoa(client.sin_addr) << ":"
                << ntohs(client.sin_port) << "\n";

      conn.send("Hello, world!\n");

      char buffer[1024];
      ssize_t n;

      while ((n = conn.recv(buffer, sizeof(buffer))) > 0) {
        std::cout << "Received " << n << " bytes\n";
        utils::dump({reinterpret_cast<std::uint8_t *>(buffer), static_cast<std::size_t>(n)});
      }
    }

  } catch (const std::system_error &e) {
    std::cerr << e.what() << "\n";
  }
}