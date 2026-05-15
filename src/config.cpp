#include "config.hpp"
#include "version.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {
void print_usage(std::string_view prog) {
  std::cout << "usage: " << prog << " [options] <root-dir>\n"
            << "       " << prog << " -h | --help\n"
            << "       " << prog << " --version\n"
            << "\n"
            << "Simple Web server.\n"
            << "\n"
            << "  <root-dir>               directory to serve\n"
            << "  -p, --port <port>        port to listen on (default: " << DEFAULT_PORT << ")\n"
            << "      --host <host>        host to bind to (default: " << DEFAULT_HOST << ")\n"
            << "  -h, --help               show this help and exit\n"
            << "  --version                show version and exit\n";
}
} // namespace

std::optional<Config> parse_config(int argc, char **argv) {
  std::string_view prog = argc > 0 ? argv[0] : "botuvera";

  for (int i = 1; i < argc; ++i) {
    std::string_view a = argv[i];
    if (a == "-h" || a == "--help") {
      print_usage(prog);
      std::exit(0);
    }
    if (a == "-v" || a == "--version") {
      std::cout << botuvera::NAME << " " << botuvera::VERSION << "\n";
      std::exit(0);
    }
  }

  Config cfg{};

  for (int i = 1; i < argc; ++i) {
    std::string_view a = argv[i];

    if (a == "--host") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        return std::nullopt;
      }
      cfg.host = argv[++i];
      continue;
    }

    if (a == "-p" || a == "--port") {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        return std::nullopt;
      }
      char *end;
      long val = std::strtol(argv[++i], &end, 10);
      if (*end != '\0' || val <= 0 || val > 65535) {
        std::cerr << "invalid port: " << argv[i] << "\n";
        return std::nullopt;
      }
      cfg.port = static_cast<uint16_t>(val);
      continue;
    }

    if (!cfg.root.empty()) {
      std::cerr << "unexpected argument: " << a << "\n";
      print_usage(prog);
      return std::nullopt;
    }

    std::error_code ec;
    cfg.root = fs::canonical(a, ec);
    if (ec || !fs::is_directory(cfg.root)) {
      std::cerr << "invalid root directory: " << a << "\n";
      return std::nullopt;
    }
  }

  if (cfg.root.empty()) {
    print_usage(prog);
    return std::nullopt;
  }

  return cfg;
}
