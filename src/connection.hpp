#pragma once

#include <cstdio>
#include <string_view>

enum class IOResult { Done, WantRead, WantWrite, Error };

class Connection {
  public:
    virtual void send(std::string_view data) = 0;
    virtual ssize_t recv(void *buffer, size_t size) = 0;
    virtual ~Connection() = default;
};