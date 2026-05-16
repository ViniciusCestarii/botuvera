#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>

enum class RequestMethod : uint8_t { UNKNOWN, GET, POST, HEAD, PUT, OPTIONS };
enum class HTTPVersion : uint8_t { UNKNOWN, HTTP_1_0, HTTP_1_1 };

class HTTPRequest {
public:
  HTTPRequest(std::string_view data);

  std::string_view get_path() const { return path_; }
  std::string_view get_host() const { return host_; }
  RequestMethod get_method() const { return method_; }
  HTTPVersion get_version() const { return version_; }

private:
  RequestMethod method_ = RequestMethod::UNKNOWN;
  HTTPVersion version_ = HTTPVersion::UNKNOWN;
  std::string path_;
  std::string host_;

  void parse_request_line(std::string_view line);
  void parse_header(std::string_view line);

  static RequestMethod parse_method(std::string_view s);
  static HTTPVersion parse_version(std::string_view s);
};

std::ostream &operator<<(std::ostream &os, const HTTPRequest &req);

enum class HTTPStatus : uint16_t {
  OK = 200,
  NotFound = 404,
  MethodNotAllowed = 405,
  InternalServerError = 500,
  VersionNotSupported = 505
};
class HTTPResponse {
public:
  HTTPResponse();

  HTTPResponse &set_version(HTTPVersion version) {
    version_ = version;
    return *this;
  };
  HTTPResponse &set_status(HTTPStatus status) {
    status_ = status;
    return *this;
  };
  HTTPResponse &set_header(std::string key, std::string value) {
    headers_[std::move(key)] = std::move(value);
    return *this;
  };
  HTTPResponse &set_body(std::string body) {
    body_ = std::move(body);
    set_header("Content-Length", std::to_string(body_.size()));
    return *this;
  };
  HTTPResponse &suppress_body() {
    omit_body_ = true;
    return *this;
  };

  std::string to_network_string() const;

private:
  HTTPVersion version_ = HTTPVersion::HTTP_1_0;
  HTTPStatus status_ = HTTPStatus::OK;
  std::unordered_map<std::string, std::string> headers_;
  std::string body_;
  bool omit_body_ = false;
};