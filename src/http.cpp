#include "http.hpp"
#include <iostream>
#include <ostream>
#include <stdexcept>

static void strip_cr(std::string &s) {
  if (!s.empty() && s.back() == '\r')
    s.pop_back();
}

std::istream &operator>>(std::istream &is, HTTPRequest &req) {
  req.parse(is);
  return is;
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
  switch (v) {
  case HTTPVersion::HTTP_1_1:
    return os << "HTTP/1.1";
  case HTTPVersion::UNKNOWN:
    return os << "UNKNOWN";
  }
  return os << "UNKNOWN";
}

std::ostream &operator<<(std::ostream &os, const HTTPRequest &req) {
  os << "Method:  " << req.get_method() << "\n"
     << "Version: " << req.get_version() << "\n"
     << "Path:    " << req.get_path() << "\n"
     << "Host:    " << req.get_host() << "\n";
  return os;
}

void HTTPRequest::parse(std::istream &is) {
  std::string line;
  if (!std::getline(is, line))
    throw std::runtime_error("Empty request");
  strip_cr(line);
  parse_request_line(line);
  while (std::getline(is, line)) {
    strip_cr(line);
    if (line.empty())
      break;
    parse_header(line);
  }
}

void HTTPRequest::parse_request_line(std::string_view line) {
  auto method_end = line.find(' ');
  auto path_end = line.find(' ', method_end + 1);
  method = parse_method(line.substr(0, method_end));
  path = line.substr(method_end + 1, path_end - method_end - 1);
  version = parse_version(line.substr(path_end + 1));
}

void HTTPRequest::parse_header(std::string_view line) {
  auto colon = line.find(':');
  if (colon == std::string_view::npos)
    return;
  auto key = line.substr(0, colon);
  auto value = line.substr(colon + 2);
  if (key == "Host")
    host = std::string(value);
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
  if (s == "HTTP/1.1")
    return HTTPVersion::HTTP_1_1;
  return HTTPVersion::UNKNOWN;
}