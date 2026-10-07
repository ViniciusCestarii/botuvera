#pragma once

#include <cerrno>
#include <chrono>
#include <coroutine>
#include <exception>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <unordered_map>

struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() noexcept {
            try {
                throw;
            } catch (const std::exception &e) {
                std::cerr << e.what() << "\n";
            } catch (...) {
            }
        }
    };
};

class EventLoop {
public:
    static EventLoop &instance() {
        static EventLoop loop;
        return loop;
    }

    void watch_read(int fd, std::coroutine_handle<> h) {
        readers_[fd] = h;
        arm(fd, EPOLLIN);
    }

    void watch_write(int fd, std::coroutine_handle<> h) {
        writers_[fd] = h;
        arm(fd, EPOLLOUT);
    }

    void remove(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        readers_.erase(fd);
        writers_.erase(fd);
        deadlines_.erase(fd);
    }

    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }

    void set_deadline(int fd) {
        deadlines_[fd] = std::chrono::steady_clock::now() + timeout_;
    }

    void run() {
        while (run_once()) {}
    }

    // Handles one batch of ready fds and expired deadlines. Returns false once
    // epoll itself has failed and the loop can't continue.
    bool run_once(int timeout_ms = 1000) {
        epoll_event events[64];
        int n = epoll_wait(epfd_, events, 64, timeout_ms);
        if (n < 0)
            return errno == EINTR;
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            if (ev & (EPOLLIN | EPOLLHUP | EPOLLERR)) {
                if (auto it = readers_.find(fd); it != readers_.end()) {
                    auto h = it->second;
                    readers_.erase(it);
                    h.resume();
                }
            }
            if (ev & (EPOLLOUT | EPOLLHUP | EPOLLERR)) {
                if (auto it = writers_.find(fd); it != writers_.end()) {
                    auto h = it->second;
                    writers_.erase(it);
                    h.resume();
                }
            }
        }
        expire_deadlines();
        return true;
    }

private:
    EventLoop() : epfd_(epoll_create1(0)) {
        if (epfd_ < 0)
            throw std::system_error(errno, std::generic_category(), "epoll_create1");
    }
    ~EventLoop() { close(epfd_); }

    // shutdown() makes the fd report EPOLLHUP, so the waiting coroutine
    // resumes, sees its read or write fail and closes the connection itself.
    void expire_deadlines() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = deadlines_.begin(); it != deadlines_.end();) {
            if (it->second <= now) {
                ::shutdown(it->first, SHUT_RDWR);
                it = deadlines_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void arm(int fd, uint32_t events) {
        epoll_event ev{};
        ev.events = events | EPOLLONESHOT;
        ev.data.fd = fd;
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) == -1 && errno == EEXIST)
            epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
    }

    int epfd_;
    std::unordered_map<int, std::coroutine_handle<>> readers_;
    std::unordered_map<int, std::coroutine_handle<>> writers_;
    std::unordered_map<int, std::chrono::steady_clock::time_point> deadlines_;
    std::chrono::milliseconds timeout_{std::chrono::seconds(10)};
};

struct ReadReady {
    int fd;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) const {
        EventLoop::instance().watch_read(fd, h);
    }
    void await_resume() const noexcept {}
};

struct WriteReady {
    int fd;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) const {
        EventLoop::instance().watch_write(fd, h);
    }
    void await_resume() const noexcept {}
};
