#pragma once

#include "connection.hpp"

#include <cstdint>
#include <netinet/in.h>
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
  void bind(const std::string &host, uint16_t port);
  void listen(int backlog = 5);
  void send(std::string_view data) override;
  ssize_t recv(void *buffer, size_t size) override;
  TCPSocket accept(sockaddr_in &client);
  int release();

private:
  int fd_;
};