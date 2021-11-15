#ifndef IVANP_SOCKET_HH
#define IVANP_SOCKET_HH

#include <string_view>
#include <utility>

namespace ivanp {

class socket {
protected:
  int fd = -1;

public:
  socket() noexcept = default;
  socket(int fd) noexcept: fd(fd) { }
  socket& operator=(int fd) noexcept {
    this->fd = fd;
    return *this;
  }
  socket(const socket&) noexcept = default;
  socket& operator=(const socket&) noexcept = default;

  bool operator==(int fd) const noexcept { return this->fd == fd; }
  bool operator!=(int fd) const noexcept { return this->fd != fd; }

  operator int() const noexcept { return fd; }

  void write(const char* data, size_t size) const;
  void write(std::string_view s) const { write(s.data(),s.size()); }
  socket operator<<(std::string_view buffer) const {
    write(buffer);
    return *this;
  }

  size_t read(char* buffer, size_t size) const;

  void sendfile(int fd, size_t size) const;

  void close() const noexcept;
};

struct unique_socket: socket {
  using socket::socket;
  ~unique_socket() { close(); }
};

} // end namespace ivanp

#endif
