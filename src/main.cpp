#include "http.hpp"
#include "socket.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <sstream>

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
      char buffer[1024];
      ssize_t n;

      while ((n = conn.recv(buffer, sizeof(buffer))) > 0) {
        std::cout << buffer;
        std::istringstream stream(std::string(buffer, n));
        HTTPRequest req;
        stream >> req;
        std::cout << "Received " << n << " bytes\n";
        std::cout << req;
      }
    }

  } catch (const std::system_error &e) {
    std::cerr << e.what() << "\n";
  }
}