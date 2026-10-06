#include "session.hpp"
#include "socket.hpp"
#include "static_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace {

// Creates and tears down a scratch directory with one served file.
class TempServeRoot {
public:
  TempServeRoot()
      : dir_(std::filesystem::temp_directory_path() /
              ("botuvera_e2e_test_" + std::to_string(::getpid()))) {
    std::filesystem::create_directories(dir_);
    std::ofstream(dir_ / "index.html") << "<html>hi</html>";
  }
  ~TempServeRoot() { std::filesystem::remove_all(dir_); }
  const std::filesystem::path &path() const { return dir_; }

private:
  std::filesystem::path dir_;
};

uint16_t bound_port(int fd) {
  sockaddr_in addr{};
  socklen_t len = sizeof(addr);
  if (::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) == -1)
    throw std::system_error(errno, std::generic_category(), "getsockname");
  return ntohs(addr.sin_port);
}

// Drives serve_connection() over a real loopback socket pair and returns
// whatever the server wrote back.
//
// Both the accepted socket and the plain client fd are left in blocking
// mode on purpose: serve_connection's poll_recv/poll_send fall back to
// plain blocking recv()/send() in that case, so the coroutine never needs
// to suspend on ReadReady/WriteReady and this can be driven synchronously
// without running the real EventLoop.
class SessionE2ETest : public ::testing::Test {
protected:
  void SetUp() override {
    file_server_.emplace(root_.path());
    listener_.reuse_address();
    listener_.bind("127.0.0.1", 0);
    listener_.listen();
  }

  std::string send_and_collect(const std::string &request) {
    uint16_t port = bound_port(listener_.fd());

    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(client_fd, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    EXPECT_EQ(::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr), 1);
    EXPECT_EQ(::connect(client_fd, reinterpret_cast<sockaddr *>(&addr),
                        sizeof(addr)),
              0);

    EXPECT_EQ(::send(client_fd, request.data(), request.size(), 0),
              static_cast<ssize_t>(request.size()));

    sockaddr_in client_addr{};
    TCPSocket accepted = listener_.accept(client_addr);
    serve_connection(std::move(accepted), *file_server_);

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = ::recv(client_fd, buf, sizeof(buf), 0)) > 0)
      response.append(buf, n);
    ::close(client_fd);
    return response;
  }

  TempServeRoot root_;
  std::optional<StaticFileServer> file_server_;
  TCPSocket listener_;
};

std::string status_line_at(const std::string &response, size_t from) {
  auto start = response.find("HTTP/1.", from);
  if (start == std::string::npos)
    return "";
  auto end = response.find("\r\n", start);
  return response.substr(start, end - start);
}

} // namespace

TEST_F(SessionE2ETest, ServesKnownFile) {
  std::string response = send_and_collect("GET /index.html HTTP/1.1\r\n"
                                           "Host: 127.0.0.1\r\n"
                                           "Connection: close\r\n"
                                           "\r\n");
  EXPECT_EQ(status_line_at(response, 0), "HTTP/1.1 200 OK");
  EXPECT_NE(response.find("<html>hi</html>"), std::string::npos);
}

TEST_F(SessionE2ETest, ReturnsNotFoundForUnknownPath) {
  std::string response = send_and_collect("GET /missing.html HTTP/1.1\r\n"
                                           "Host: 127.0.0.1\r\n"
                                           "Connection: close\r\n"
                                           "\r\n");
  EXPECT_EQ(status_line_at(response, 0), "HTTP/1.1 404 Not Found");
}

TEST_F(SessionE2ETest, RejectsUnsupportedMethod) {
  std::string response = send_and_collect("DELETE /index.html HTTP/1.1\r\n"
                                           "Host: 127.0.0.1\r\n"
                                           "Connection: close\r\n"
                                           "\r\n");
  EXPECT_EQ(status_line_at(response, 0), "HTTP/1.1 405 Method Not Allowed");
}

TEST_F(SessionE2ETest, MalformedRequestGetsCleanErrorInsteadOfCrashing) {
  std::string response = send_and_collect("GET\r\n\r\n");
  EXPECT_EQ(status_line_at(response, 0), "HTTP/1.1 400 Bad Request");
}

TEST_F(SessionE2ETest, HandlesTwoPipelinedKeepAliveRequests) {
  const std::string request = "GET /index.html HTTP/1.1\r\n"
                               "Host: 127.0.0.1\r\n"
                               "Connection: keep-alive\r\n"
                               "\r\n"
                               "GET /index.html HTTP/1.1\r\n"
                               "Host: 127.0.0.1\r\n"
                               "Connection: close\r\n"
                               "\r\n";
  std::string response = send_and_collect(request);

  std::string first = status_line_at(response, 0);
  ASSERT_EQ(first, "HTTP/1.1 200 OK");
  auto after_first = response.find("\r\n", response.find(first));
  std::string second = status_line_at(response, after_first);
  EXPECT_EQ(second, "HTTP/1.1 200 OK") << "full response:\n" << response;
}

TEST_F(SessionE2ETest, RequestBodyDoesNotLeakIntoNextPipelinedRequest) {
  const std::string request = "POST /index.html HTTP/1.1\r\n"
                               "Host: 127.0.0.1\r\n"
                               "Content-Length: 5\r\n"
                               "Connection: keep-alive\r\n"
                               "\r\n"
                               "hello"
                               "GET /index.html HTTP/1.1\r\n"
                               "Host: 127.0.0.1\r\n"
                               "Connection: close\r\n"
                               "\r\n";
  std::string response = send_and_collect(request);

  std::string first = status_line_at(response, 0);
  ASSERT_EQ(first, "HTTP/1.1 405 Method Not Allowed");
  auto after_first = response.find("\r\n", response.find(first));
  std::string second = status_line_at(response, after_first);
  EXPECT_EQ(second, "HTTP/1.1 200 OK") << "full response:\n" << response;
}
