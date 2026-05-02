cmake_minimum_required(VERSION 3.30)
project(botuvera LANGUAGES CXX)

add_executable(botuvera
  src/main.cpp
  src/socket.cpp
)

target_compile_features(botuvera PRIVATE cxx_std_23)