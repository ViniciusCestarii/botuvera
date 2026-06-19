#pragma once

#include "http.hpp"
#include <filesystem>
#include <string>
#include <sys/types.h>
#include <unordered_map>

class StaticFileServer {
public:
  explicit StaticFileServer(std::filesystem::path root, uint max_age = 0,
                            uint html_max_age = 0);

  HTTPResponse serve(const HTTPRequest &req) const;

private:
  struct CachedFile {
    std::string body;
    std::string content_type;
    std::string etag;
  };

  std::filesystem::path root_;
  std::unordered_map<std::string, CachedFile> cache_;
  std::string cache_control_;
  std::string cache_control_html_;

  const CachedFile *lookup(std::string_view url_path) const;
};
