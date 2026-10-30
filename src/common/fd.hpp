#pragma once
#include <unistd.h>
namespace minic::detail {
class Fd {
    int fd_ = -1;
public:
    explicit Fd(int fd = -1) noexcept : fd_(fd) {}
    ~Fd() { reset(); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const { return fd_; }
    void reset(int fd = -1) noexcept { if (fd_ >= 0) ::close(fd_); fd_ = fd; }
};
}
