cmake_minimum_required(VERSION 3.30)
project(botuvera LANGUAGES CXX)

find_package(OpenSSL REQUIRED)

add_library(botuvera_lib STATIC
  src/socket.cpp
  src/http.cpp
  src/static_server.cpp
  src/config.cpp
  src/tls.cpp
)
target_include_directories(botuvera_lib PUBLIC src)
target_compile_features(botuvera_lib PUBLIC cxx_std_23)
target_link_libraries(botuvera_lib PUBLIC pthread OpenSSL::SSL OpenSSL::Crypto)

add_executable(botuvera src/main.cpp)
target_link_libraries(botuvera PRIVATE botuvera_lib)

enable_testing()
add_subdirectory(tests)
