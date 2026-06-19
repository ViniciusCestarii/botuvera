#include "tls.hpp"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>

TLSServer::TLSServer() {
  ctx_ = SSL_CTX_new(TLS_server_method());
  if (!ctx_) {
    const char *reason = ERR_reason_error_string(ERR_get_error());
    throw std::runtime_error(reason ? reason : "SSL_CTX_new failed");
  }
}

void TLSServer::configure_ctx(const std::filesystem::path &cert_file,
                              const std::filesystem::path &key_file) {
  SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER);

  if (SSL_CTX_use_certificate_chain_file(ctx_, cert_file.c_str()) <= 0) {
    const char *reason = ERR_reason_error_string(ERR_get_error());
    throw std::runtime_error(reason ? reason
                                    : "SSL_CTX_use_certificate_file failed");
  }

  if (SSL_CTX_use_PrivateKey_file(ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <=
      0) {
    const char *reason = ERR_reason_error_string(ERR_get_error());
    throw std::runtime_error(reason ? reason
                                    : "SSL_CTX_use_PrivateKey_file failed");
  }
}

TLSServer::~TLSServer() { SSL_CTX_free(ctx_); }

TLSConnection::TLSConnection(int fd, SSL_CTX *ctx) : fd_(fd), ssl_(SSL_new(ctx)) {
  if (!ssl_)
    throw std::runtime_error("SSL_new failed");
  SSL_set_fd(ssl_, fd_);
}

TLSPoll TLSConnection::poll_handshake() {
  int ret = SSL_accept(ssl_);
  if (ret == 1)
    return TLSPoll::Done;
  int err = SSL_get_error(ssl_, ret);
  if (err == SSL_ERROR_WANT_READ)
    return TLSPoll::WantRead;
  if (err == SSL_ERROR_WANT_WRITE)
    return TLSPoll::WantWrite;
  return TLSPoll::Error;
}

TLSPoll TLSConnection::poll_recv(void *buf, size_t size, ssize_t &out) {
  int n = SSL_read(ssl_, buf, static_cast<int>(size));
  if (n > 0) {
    out = n;
    return TLSPoll::Done;
  }
  out = 0;
  int err = SSL_get_error(ssl_, n);
  if (err == SSL_ERROR_WANT_READ)
    return TLSPoll::WantRead;
  if (err == SSL_ERROR_WANT_WRITE)
    return TLSPoll::WantWrite;
  return TLSPoll::Error;
}

TLSPoll TLSConnection::poll_send(const char *&p, size_t &remaining) {
  int w = SSL_write(ssl_, p, static_cast<int>(remaining));
  if (w > 0) {
    p += w;
    remaining -= w;
    return TLSPoll::Done;
  }
  int err = SSL_get_error(ssl_, w);
  if (err == SSL_ERROR_WANT_WRITE)
    return TLSPoll::WantWrite;
  if (err == SSL_ERROR_WANT_READ)
    return TLSPoll::WantRead;
  return TLSPoll::Error;
}

void TLSConnection::send(std::string_view data) {
  const char *p = data.data();
  size_t remaining = data.size();
  while (remaining > 0) {
    int n = SSL_write(ssl_, p, remaining);
    if (n <= 0) {
      const char *reason = ERR_reason_error_string(ERR_get_error());
      throw std::runtime_error(reason ? reason : "SSL_write failed");
    }
    p += n;
    remaining -= n;
  }
}

ssize_t TLSConnection::recv(void *buffer, size_t size) {
  int n = SSL_read(ssl_, buffer, size);
  if (n <= 0) {
    const char *reason = ERR_reason_error_string(ERR_get_error());
    throw std::runtime_error(reason ? reason : "SSL_read failed");
  }
  return n;
}

TLSConnection::~TLSConnection() {
  SSL_shutdown(ssl_);
  SSL_free(ssl_);
  close(fd_);
}
