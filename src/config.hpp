#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

constexpr uint16_t DEFAULT_PORT = 7432;
constexpr uint16_t DEFAULT_TLS_PORT = 8432;
constexpr const char *DEFAULT_HOST = "127.0.0.1";

struct Config {
  std::filesystem::path root;
  std::string host = DEFAULT_HOST;
  uint16_t port = DEFAULT_PORT;
  uint16_t tls_port = DEFAULT_TLS_PORT;
  std::optional<std::filesystem::path> cert_path;
  std::optional<std::filesystem::path> key_path;
};

std::optional<Config> parse_config(int argc, char **argv);
