#pragma once

#include <ostream>
#include <string>
#include <string_view>

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