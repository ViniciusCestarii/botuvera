#pragma once

#include "connection.hpp"

#include <filesystem>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

class TLSConnection: public Connection {
  public:
    TLSConnection(int fd, SSL_CTX *ctx);

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

    SSL_CTX *get_ctx() { return ctx_;};
    void configure_ctx(const std::filesystem::path& cert_file, const std::filesystem::path& key_file);

    ~TLSServer();

  private:
    SSL_CTX *ctx_;
};