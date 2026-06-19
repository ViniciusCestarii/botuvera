#pragma once

#include "http.hpp"
#include <filesystem>
#include <string>
#include <unordered_map>

class StaticFileServer {
public:
  explicit StaticFileServer(std::filesystem::path root);

  HTTPResponse serve(const HTTPRequest &req) const;

private:
  struct CachedFile {
    std::string body;
    std::string content_type;
  };

  std::filesystem::path root_;
  std::unordered_map<std::string, CachedFile> cache_;

  const CachedFile *lookup(std::string_view url_path) const;
};
