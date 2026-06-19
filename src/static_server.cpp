#include "static_server.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/types.h>

namespace fs = std::filesystem;

namespace {

std::string content_type_for(const fs::path &p) {
  auto ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (ext == ".html" || ext == ".htm")
    return "text/html; charset=utf-8";
  if (ext == ".css")
    return "text/css; charset=utf-8";
  if (ext == ".js")
    return "application/javascript; charset=utf-8";
  if (ext == ".json")
    return "application/json; charset=utf-8";
  if (ext == ".txt")
    return "text/plain; charset=utf-8";
  if (ext == ".xml")
    return "application/xml; charset=utf-8";
  if (ext == ".png")
    return "image/png";
  if (ext == ".jpg" || ext == ".jpeg")
    return "image/jpeg";
  if (ext == ".gif")
    return "image/gif";
  if (ext == ".svg")
    return "image/svg+xml";
  if (ext == ".ico")
    return "image/x-icon";
  if (ext == ".wasm")
    return "application/wasm";
  return "application/octet-stream";
}

std::string cache_control_for(uint max_age) {
  return max_age == 0 ? "no-cache" : "max-age=" + std::to_string(max_age);
}

std::string make_etag(std::string_view body) {
  auto hash = std::hash<std::string_view>{}(body);
  char buf[2 + 16 + 1];
  std::snprintf(buf, sizeof(buf), "\"%016zx\"", hash);
  return buf;
}

HTTPResponse make_404(bool suppress) {
  HTTPResponse r;
  r.set_status(HTTPStatus::NotFound)
      .set_header("Content-Type", "text/html; charset=utf-8")
      .set_body("<html><body><h1>404 Not Found</h1></body></html>");
  if (suppress)
    r.suppress_body();
  return r;
}

} // namespace

StaticFileServer::StaticFileServer(fs::path root, uint max_age,
                                   uint html_max_age)
    : root_(std::move(root)), cache_control_(cache_control_for(max_age)),
      cache_control_html_(cache_control_for(html_max_age)) {
  std::error_code ec;
  for (const auto &entry :
       fs::recursive_directory_iterator(root_, ec)) {
    if (!entry.is_regular_file())
      continue;

    auto rel = fs::relative(entry.path(), root_, ec);
    if (ec)
      continue;

    std::ifstream f(entry.path(), std::ios::binary | std::ios::ate);
    if (!f)
      continue;
    auto size = f.tellg();
    if (size < 0)
      continue;
    std::string body(static_cast<size_t>(size), '\0');
    f.seekg(0);
    if (!f.read(body.data(), size))
      continue;

    auto key = rel.generic_string();
    auto etag = make_etag(body);
    cache_[key] = {std::move(body), content_type_for(entry.path()),
                   std::move(etag)};
  }
  std::cout << "Cached " << cache_.size() << " file(s) from " << root_ << "\n";
}

const StaticFileServer::CachedFile *
StaticFileServer::lookup(std::string_view url_path) const {
  if (auto q = url_path.find('?'); q != std::string_view::npos)
    url_path = url_path.substr(0, q);
  while (!url_path.empty() && url_path.front() == '/')
    url_path.remove_prefix(1);
  while (!url_path.empty() && url_path.back() == '/')
    url_path.remove_suffix(1);

  std::string key(url_path);
  for (const auto &candidate :
       {key, key + ".html",
        key.empty() ? std::string("index.html") : key + "/index.html"}) {
    auto it = cache_.find(candidate);
    if (it != cache_.end())
      return &it->second;
  }
  return nullptr;
}

HTTPResponse StaticFileServer::serve(const HTTPRequest &req) const {
  HTTPResponse r;

  if (req.get_version() == HTTPVersion::UNKNOWN)
    return r.set_status(HTTPStatus::VersionNotSupported);

  const auto method = req.get_method();
  if (method != RequestMethod::GET && method != RequestMethod::HEAD)
    return r.set_status(HTTPStatus::MethodNotAllowed)
        .set_header("Allow", "GET, HEAD");

  const bool is_head = method == RequestMethod::HEAD;

  const auto *cached = lookup(req.get_path());
  if (!cached)
    return make_404(is_head);

  const bool is_html = cached->content_type.rfind("text/html", 0) == 0;
  r.set_header("ETag", cached->etag)
      .set_header("Cache-Control",
                  is_html ? cache_control_html_ : cache_control_);

  if (req.get_if_none_match() == cached->etag)
    return r.set_status(HTTPStatus::NotModified).suppress_body();

  r.set_status(HTTPStatus::OK).set_header("Content-Type", cached->content_type);
  if (is_head)
    return r.set_header("Content-Length",
                        std::to_string(cached->body.size()))
        .suppress_body();

  return r.set_body(cached->body);
}
