#include "http.hpp"
#include "socket.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <string_view>

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
      std::string buf;
      char chunk[1024];
      ssize_t n;

      while ((n = conn.recv(chunk, sizeof(chunk))) > 0) {
        buf.append(chunk, n);

        if (auto end = buf.find("\r\n\r\n"); end != std::string::npos) {
          std::cout << buf;

          HTTPRequest req(std::string_view(buf.data(), end + 4));

          if (req.get_version() == HTTPVersion::UNKNOWN) {
            std::string_view resp =
                "HTTP/1.0 505 HTTP Version Not Supported\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n"
                "Server: botuvera/0.1\r\n"
                "\r\n";
            conn.send(resp);
            break;
          }

          if (req.get_method() != RequestMethod::GET) {
            std::string_view resp = "HTTP/1.0 405 Method Not Allowed\r\n"
                                    "Content-Length: 0\r\n"
                                    "Allow: GET\r\n"
                                    "Connection: close\r\n"
                                    "Server: botuvera/0.1\r\n"
                                    "\r\n";
            conn.send(resp);
            break;
          }

          std::string_view resp = "HTTP/1.0 200 OK\r\n"
                                  "Content-Length: 60\r\n"
                                  "Connection: close\r\n"
                                  "Server: botuvera/0.1\r\n"
                                  "\r\n"
                                  "<html>"
                                  "<body>"
                                  "<h1>200 OK</h1>"
                                  "<p>Basic Answer</p>"
                                  "</body>"
                                  "</html>";
          conn.send(resp);

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