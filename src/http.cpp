#include "http.hpp"
#include "version.hpp"
#include <ostream>
#include <stdexcept>
#include <string_view>

namespace {
std::string_view version_to_sv(HTTPVersion v) {
  switch (v) {
  case HTTPVersion::HTTP_1_0:
    return "HTTP/1.0";
  case HTTPVersion::HTTP_1_1:
    return "HTTP/1.1";
  case HTTPVersion::UNKNOWN:
    return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::ostream &operator<<(std::ostream &os, RequestMethod m) {
  switch (m) {
  case RequestMethod::GET:
    return os << "GET";
  case RequestMethod::POST:
    return os << "POST";
  case RequestMethod::HEAD:
    return os << "HEAD";
  case RequestMethod::PUT:
    return os << "PUT";
  case RequestMethod::OPTIONS:
    return os << "OPTIONS";
  case RequestMethod::UNKNOWN:
    return os << "UNKNOWN";
  }
  return os << "UNKNOWN";
}

std::ostream &operator<<(std::ostream &os, HTTPVersion v) {
  return os << version_to_sv(v);
}

std::string_view status_to_reason_sv(HTTPStatus s) {
  switch (s) {
  case HTTPStatus::OK:
    return "OK";
  case HTTPStatus::NotModified:
    return "Not Modified";
  case HTTPStatus::NotFound:
    return "Not Found";
  case HTTPStatus::MethodNotAllowed:
    return "Method Not Allowed";
  case HTTPStatus::InternalServerError:
    return "Internal Server Error";
  case HTTPStatus::VersionNotSupported:
    return "HTTP Version Not Supported";
  }
  return "Unknown";
};
} // namespace

std::ostream &operator<<(std::ostream &os, const HTTPRequest &req) {
  os << "Method:  " << req.get_method() << "\n"
     << "Version: " << req.get_version() << "\n"
     << "Path:    " << req.get_path() << "\n"
     << "Host:    " << req.get_host() << "\n";
  return os;
}

HTTPRequest::HTTPRequest(std::string_view data) {
  size_t pos = 0;
  auto next_line = [&]() -> std::string_view {
    auto eol = data.find("\r\n", pos);
    auto line = data.substr(pos, eol - pos);
    pos = (eol == std::string_view::npos) ? data.size() : eol + 2;
    return line;
  };

  auto line = next_line();
  if (line.empty())
    throw std::runtime_error("Empty request");
  parse_request_line(line);

  while (pos < data.size()) {
    auto h = next_line();
    if (h.empty())
      break;
    parse_header(h);
  }
}

void HTTPRequest::parse_request_line(std::string_view line) {
  auto method_end = line.find(' ');
  auto path_end = line.find(' ', method_end + 1);
  method_ = parse_method(line.substr(0, method_end));
  path_ = line.substr(method_end + 1, path_end - method_end - 1);
  version_ = parse_version(line.substr(path_end + 1));
}

void HTTPRequest::parse_header(std::string_view line) {
  auto colon = line.find(':');
  if (colon == std::string_view::npos)
    return;
  auto key = line.substr(0, colon);
  auto value = line.substr(colon + 2);
  if (key == "Host") {
    host_ = std::string(value);
  } else if (key == "Connection") {
    connection_ = std::string(value);
    std::transform(connection_.begin(), connection_.end(), connection_.begin(),
                   [](unsigned char c) { return std::tolower(c); });
  } else if (key == "If-None-Match") {
    if_none_match_ = std::string(value);
  }
}

bool HTTPRequest::wants_keep_alive() const {
  if (connection_ == "close")
    return false;
  if (version_ == HTTPVersion::HTTP_1_1)
    return true;
  return connection_ == "keep-alive";
}

RequestMethod HTTPRequest::parse_method(std::string_view s) {
  if (s == "GET")
    return RequestMethod::GET;
  if (s == "POST")
    return RequestMethod::POST;
  if (s == "HEAD")
    return RequestMethod::HEAD;
  if (s == "PUT")
    return RequestMethod::PUT;
  if (s == "OPTIONS")
    return RequestMethod::OPTIONS;
  return RequestMethod::UNKNOWN;
}

HTTPVersion HTTPRequest::parse_version(std::string_view s) {
  if (s == "HTTP/1.0")
    return HTTPVersion::HTTP_1_0;
  if (s == "HTTP/1.1")
    return HTTPVersion::HTTP_1_1;
  return HTTPVersion::UNKNOWN;
}

HTTPResponse::HTTPResponse()
    : headers_{
          {"Content-Length", "0"},
          {"Connection", "close"},
          {"Server", std::string(botuvera::USER_AGENT)},
      } {}

std::string HTTPResponse::to_network_string() const {
  std::string result;
  result.reserve(256 + (omit_body_ ? 0 : body_.size()));  // rough guess

  // status line
  result += version_to_sv(version_);
  result += " ";
  result += std::to_string(static_cast<int>(status_));
  result += " ";
  result += std::string(status_to_reason_sv(status_));
  result += "\r\n";

  // headers
  for (const auto &[key, value] : headers_) {
    result += key + ": " + value + "\r\n";
  }

  result += "\r\n";

  // body
  if (!omit_body_) {
    result += body_;
  }

  return result;
}