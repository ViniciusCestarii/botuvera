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

            auto resp = HTTPResponse()
                            .set_version(HTTPVersion::HTTP_1_0)
                            .set_status(HTTPStatus::VersionNotSupported);

            conn.send(resp.to_network_string());
            break;
          }

          if (req.get_method() != RequestMethod::GET) {
            auto resp = HTTPResponse()
                            .set_version(HTTPVersion::HTTP_1_0)
                            .set_status(HTTPStatus::MethodNotAllowed)
                            .set_header("Allow", "GET");
            conn.send(resp.to_network_string());
            break;
          }

          auto resp = HTTPResponse()
                          .set_version(HTTPVersion::HTTP_1_0)
                          .set_status(HTTPStatus::OK)
                          .set_body("<html>"
                                    "<body>"
                                    "<h1>200 OK</h1>"
                                    "<p>Basic Answer</p>"
                                    "</body>"
                                    "</html>");
          conn.send(resp.to_network_string());

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