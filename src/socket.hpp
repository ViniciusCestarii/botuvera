#pragma once

#include "connection.hpp"

#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>
class TCPSocket: public Connection {
public:
  TCPSocket();
  explicit TCPSocket(int fd);

  TCPSocket(TCPSocket &&other) noexcept;
  TCPSocket(const TCPSocket &) = delete;

  TCPSocket &operator=(const TCPSocket &) = delete;
  TCPSocket &operator=(TCPSocket &&) noexcept;

  ~TCPSocket();

  void reuse_address();
  void set_nonblocking();
  void bind(const std::string &host, uint16_t port);
  void listen(int backlog = SOMAXCONN);
  void send(std::string_view data) override;
  ssize_t recv(void *buffer, size_t size) override;
  TCPSocket accept(sockaddr_in &client);
  int fd() const { return fd_; }
  int release();

private:
  int fd_;
};