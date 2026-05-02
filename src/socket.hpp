#pragma once

#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <string_view>

class TCPSocket {
public:
  TCPSocket();
  explicit TCPSocket(int fd);

  TCPSocket(TCPSocket &&other) noexcept;
  TCPSocket(const TCPSocket &) = delete;

  TCPSocket &operator=(const TCPSocket &) = delete;
  TCPSocket &operator=(TCPSocket &&) noexcept;

  ~TCPSocket();

  void reuse_address();
  void bind(uint16_t port);
  void listen(int backlog = 5);
  TCPSocket accept(sockaddr_in &client);
  void send(std::string_view data);
  ssize_t recv(void *buffer, size_t size);

private:
  int fd_;
};