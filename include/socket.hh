#ifndef IVANP_SOCKET_HH
#define IVANP_SOCKET_HH

#include <string_view>
#include <utility>

#include <sys/types.h>

#include "int_fd.hh"

namespace ivanp {

struct socket: int_fd {
  using int_fd::int_fd;

  void write(const char* data, size_t size) const;
  void write(std::string_view s) const { write(s.data(),s.size()); }
  socket operator<<(std::string_view buffer) const {
    write(buffer);
    return *this;
  }

  size_t read(char* buffer, size_t size) const;

  void sendfile(int fd, size_t size, off_t offset=0) const;

  void close() const noexcept;
};

struct unique_socket: socket {
  using socket::socket;
  ~unique_socket() { close(); }
};

} // end namespace ivanp

#endif
