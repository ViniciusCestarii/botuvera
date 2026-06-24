#include "socket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

TCPSocket::TCPSocket() : fd_(::socket(AF_INET, SOCK_STREAM, 0)) {
  if (fd_ == -1)
    throw std::system_error(errno, std::generic_category(), "socket");
}

TCPSocket::TCPSocket(int fd) : fd_(fd) {}

TCPSocket::TCPSocket(TCPSocket &&other) noexcept : fd_(other.fd_) {
  other.fd_ = -1;
}

TCPSocket::~TCPSocket() {
  if (fd_ != -1)
    ::close(fd_);
}

void TCPSocket::set_nonblocking() {
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags == -1)
    throw std::system_error(errno, std::generic_category(), "fcntl F_GETFL");
  if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1)
    throw std::system_error(errno, std::generic_category(), "fcntl F_SETFL");
}

void TCPSocket::reuse_address() {
  int yes = 1;

  if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    throw std::system_error(errno, std::generic_category(), "setsockopt");
}

void TCPSocket::bind(const std::string &host, std::uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    throw std::system_error(errno, std::generic_category(),
                            "invalid host: " + host);

  if (::bind(fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == -1)
    throw std::system_error(errno, std::generic_category(), "bind");
}

void TCPSocket::listen(int backlog) {
  if (::listen(fd_, backlog) == -1)
    throw std::system_error(errno, std::generic_category(), "listen failed");
}

TCPSocket TCPSocket::accept(sockaddr_in &client) {
  socklen_t len = sizeof(client);

  int client_fd = ::accept(fd_, reinterpret_cast<sockaddr *>(&client), &len);

  if (client_fd == -1)
    throw std::system_error(errno, std::generic_category(), "accept");

  return TCPSocket(client_fd);
}

void TCPSocket::send(std::string_view data) {
  const char *p = data.data();
  size_t remaining = data.size();
  while (remaining > 0) {
    ssize_t n = ::send(fd_, p, remaining, 0);
    if (n == -1)
      throw std::system_error(errno, std::generic_category(), "send");
    p += n;
    remaining -= n;
  }
}

ssize_t TCPSocket::recv(void *buffer, size_t size) {
  ssize_t n = ::recv(fd_, buffer, size, 0);
  if (n == -1)
    throw std::system_error(errno, std::generic_category(), "recv");
  return n;
}

IOResult TCPSocket::poll_recv(void *buf, size_t size, ssize_t &out) {
  ssize_t n = ::recv(fd_, buf, size, 0);
  if (n > 0) {
    out = n;
    return IOResult::Done;
  }
  out = 0;
  if (n == 0)
    return IOResult::Error; // peer closed
  if (errno == EAGAIN || errno == EWOULDBLOCK)
    return IOResult::WantRead;
  return IOResult::Error;
}

IOResult TCPSocket::poll_send(const char *&p, size_t &remaining) {
  ssize_t w = ::send(fd_, p, remaining, 0);
  if (w > 0) {
    p += w;
    remaining -= w;
    return IOResult::Done;
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK)
    return IOResult::WantWrite;
  return IOResult::Error;
}

int TCPSocket::release() {
  int fd = fd_;
  fd_ = -1;
  return fd;
}