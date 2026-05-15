#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

constexpr uint16_t DEFAULT_PORT = 7432;
constexpr const char *DEFAULT_HOST = "127.0.0.1";

struct Config {
  std::filesystem::path root;
  std::string host = DEFAULT_HOST;
  uint16_t port = DEFAULT_PORT;
};

std::optional<Config> parse_config(int argc, char **argv);
