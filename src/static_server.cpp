#include "static_server.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

std::string_view content_type_for(const fs::path &p) {
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

std::optional<fs::path> resolve_under(const fs::path &root,
                                      const fs::path &candidate) {
  std::error_code ec;
  auto canon = fs::weakly_canonical(candidate, ec);
  if (ec)
    return std::nullopt;
  if (!fs::is_regular_file(canon, ec))
    return std::nullopt;

  auto root_s = root.string();
  auto canon_s = canon.string();
  if (canon_s != root_s &&
      (canon_s.size() <= root_s.size() ||
       canon_s.compare(0, root_s.size(), root_s) != 0 ||
       canon_s[root_s.size()] != fs::path::preferred_separator)) {
    return std::nullopt;
  }
  return canon;
}

std::optional<fs::path> find_file(const fs::path &root,
                                  std::string_view url_path) {
  if (auto q = url_path.find('?'); q != std::string_view::npos)
    url_path = url_path.substr(0, q);
  while (!url_path.empty() && url_path.front() == '/')
    url_path.remove_prefix(1);

  fs::path base = url_path.empty() ? root : root / std::string(url_path);

  for (const fs::path &c :
       {base, fs::path(base.string() + ".html"), base / "index.html"}) {
    if (auto r = resolve_under(root, c))
      return r;
  }
  return std::nullopt;
}

std::optional<std::string> read_file(const fs::path &p) {
  std::ifstream f(p, std::ios::binary | std::ios::ate);
  if (!f)
    return std::nullopt;
  auto size = f.tellg();
  if (size < 0)
    return std::nullopt;
  std::string out(static_cast<size_t>(size), '\0');
  f.seekg(0);
  if (!f.read(out.data(), size))
    return std::nullopt;
  return out;
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

HTTPResponse make_500() {
  HTTPResponse r;
  return r.set_status(HTTPStatus::InternalServerError)
      .set_header("Content-Type", "text/html; charset=utf-8")
      .set_body("<html><body><h1>500 Internal Server Error</h1></body></html>");
}

} // namespace

StaticFileServer::StaticFileServer(fs::path root) : root_(std::move(root)) {}

HTTPResponse StaticFileServer::serve(const HTTPRequest &req) const {
  HTTPResponse r;

  if (req.get_version() == HTTPVersion::UNKNOWN)
    return r.set_status(HTTPStatus::VersionNotSupported);

  const auto method = req.get_method();
  if (method != RequestMethod::GET && method != RequestMethod::HEAD)
    return r.set_status(HTTPStatus::MethodNotAllowed)
        .set_header("Allow", "GET, HEAD");

  const bool is_head = method == RequestMethod::HEAD;

  auto file = find_file(root_, req.get_path());
  if (!file)
    return make_404(is_head);

  if (is_head) {
    std::error_code ec;
    auto size = fs::file_size(*file, ec);
    return r.set_status(HTTPStatus::OK)
        .set_header("Content-Type", std::string(content_type_for(*file)))
        .set_header("Content-Length", ec ? "0" : std::to_string(size))
        .suppress_body();
  }

  auto body = read_file(*file);
  if (!body)
    return make_500();

  return r.set_status(HTTPStatus::OK)
      .set_header("Content-Type", std::string(content_type_for(*file)))
      .set_body(std::move(*body));
}
