#pragma once

#include <iomanip>
#include <iostream>
#include <span>

namespace utils {
inline void dump(std::span<const std::uint8_t> data) {
  for (std::size_t i = 0; i < data.size(); ++i) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<unsigned>(data[i]) << " ";

    if (i % 16 == 15 || i == data.size() - 1) {
      for (std::size_t j = 0; j < 15 - (i % 16); ++j)
        std::cout << "   ";
      std::cout << "| ";
      for (std::size_t j = i - (i % 16); j <= i; ++j) {
        const auto byte = data[j];
        std::cout << static_cast<char>((byte > 31 && byte < 127) ? byte : '.');
      }
      std::cout << std::dec << "\n";
    }
  }
}
} // namespace utils
