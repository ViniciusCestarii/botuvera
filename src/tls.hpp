#pragma once

#include "connection.hpp"

#include <filesystem>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

enum class TLSPoll { Done, WantRead, WantWrite, Error };

class TLSConnection : public Connection {
public:
  TLSConnection(int fd, SSL_CTX *ctx);

  TLSPoll poll_handshake();
  TLSPoll poll_recv(void *buf, size_t size, ssize_t &out);
  TLSPoll poll_send(const char *&p, size_t &remaining);

  int fd() const { return fd_; }

  void send(std::string_view data) override;
  ssize_t recv(void *buffer, size_t size) override;

  ~TLSConnection();

private:
  SSL *ssl_;
  int fd_;
};

class TLSServer {
public:
  TLSServer();

  SSL_CTX *get_ctx() { return ctx_; }
  void configure_ctx(const std::filesystem::path &cert_file,
                     const std::filesystem::path &key_file);

  ~TLSServer();

private:
  SSL_CTX *ctx_;
};
