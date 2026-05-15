cmake_minimum_required(VERSION 3.30)
project(botuvera LANGUAGES CXX)

add_executable(botuvera
  src/main.cpp
  src/socket.cpp
  src/http.cpp
  src/static_server.cpp
  src/config.cpp
)

target_compile_features(botuvera PRIVATE cxx_std_23)
target_link_libraries(botuvera PRIVATE pthread)