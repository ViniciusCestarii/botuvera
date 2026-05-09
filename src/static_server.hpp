#pragma once

#include "http.hpp"
#include <filesystem>

class StaticFileServer {
public:
  explicit StaticFileServer(std::filesystem::path root);

  HTTPResponse serve(const HTTPRequest &req) const;

private:
  std::filesystem::path root_;
};
