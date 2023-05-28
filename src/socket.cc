#include "socket.hh"

#include <unistd.h>
#include <sys/sendfile.h>
#include <thread>

#include "error.hh"

namespace ivan {

size_t socket::read(char* buffer, size_t size) const {
  size_t nread = 0;
  for (;;) {
    const auto ret = ::read(fd, buffer, size);
    if (ret < 0) {
      const auto e = errno;
      if (e == EAGAIN || e == EWOULDBLOCK) {
        std::this_thread::yield();
        continue;
      } else THROW_ERRNO("read()");
    } else return nread += ret;
  }
  return nread;
}

void socket::write(const char* data, size_t size) const {
  while (size) {
    const auto ret = ::write(fd, data, size);
    if (ret < 0) {
      const auto e = errno;
      if (e == EAGAIN || e == EWOULDBLOCK) {
        std::this_thread::yield();
        continue;
      } else THROW_ERRNO("write()");
    }
    data += ret;
    size -= ret;
  }
}

void socket::sendfile(int in_fd, size_t size, off_t offset) const {
  while (size) {
    const auto sent = ::sendfile(fd, in_fd, &offset, size);
    if (sent < 0) {
      const auto e = errno;
      if (e == EAGAIN) {
        std::this_thread::yield();
        continue;
      } else THROW_ERRNO("sendfile()");
    }
    size -= sent;
  }
}

void socket::close() const noexcept {
  ::close(fd);
}

} // end namespace ivan
