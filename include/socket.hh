#ifndef IVAN_SOCKET_HH
#define IVAN_SOCKET_HH

#include <cstdint>
#include <string_view>
#include <utility>

#include <sys/types.h>

namespace ivan {

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
  operator int() const noexcept { return fd; }

  void write(const char* data, size_t size) const;
  void write(std::string_view s) const { write(s.data(),s.size()); }
  socket operator<<(std::string_view buffer) const {
    write(buffer);
    return *this;
  }

  size_t read(char* buffer, size_t size) const;

  void sendfile(int fd, size_t size, off_t offset=0) const;

  void close() const noexcept;

  uint32_t addr() const;
};

class unique_socket: public socket {
public:
  using socket::socket;

  unique_socket(unique_socket&& o) noexcept: socket(o.fd) { o.fd = -1; }
  unique_socket& operator=(unique_socket&& o) noexcept {
    fd = o.fd;
    o.fd = -1;
    return *this;
  }

  ~unique_socket() { if (fd != -1) close(); }
};

} // end namespace ivan

#endif
