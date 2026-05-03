#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <string_view>

enum class RequestMethod : uint8_t { UNKNOWN, GET, POST, HEAD, PUT, OPTIONS };
enum class HTTPVersion : uint8_t { UNKNOWN, HTTP_1_1 };

class HTTPRequest {
public:
  std::string_view get_path() const { return path; }
  std::string_view get_host() const { return host; }
  RequestMethod get_method() const { return method; }
  HTTPVersion get_version() const { return version; }

  void parse(std::istream& is);

private:
  RequestMethod method = RequestMethod::UNKNOWN;
  HTTPVersion version = HTTPVersion::UNKNOWN;
  std::string path;
  std::string host;

  void parse_request_line(std::string_view line);
  void parse_header(std::string_view line);

  static RequestMethod parse_method(std::string_view s);
  static HTTPVersion parse_version(std::string_view s);
};

std::istream &operator>>(std::istream &is, HTTPRequest &req);
std::ostream &operator<<(std::ostream &os, const HTTPRequest &req);