#ifndef IVAN_INT_FD_HH
#define IVAN_INT_FD_HH

namespace ivan {

class int_fd {
protected:
  int fd = -1;
public:
  int_fd() noexcept = default;
  int_fd(int fd) noexcept: fd(fd) { }
  int_fd& operator=(int fd) noexcept {
    this->fd = fd;
    return *this;
  }
  // int_fd(const int_fd&) noexcept = default;
  // int_fd& operator=(const int_fd&) noexcept = default;
  operator int() const noexcept { return fd; }
};

}

#endif
