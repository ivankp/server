#include "socket.hh"

#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <type_traits>

#include "error.hh"

namespace ivan {

size_t socket::read(char* buffer, size_t size) const {
  size_t nread = 0;
  for (;;) {
    // TODO: don't I need to change buffer pointer passed to read()?
    const auto ret = ::read(fd, buffer, size);
    if (ret < 0) {
      const auto e = errno;
      if (e == EAGAIN || e == EWOULDBLOCK) {
        std::this_thread::yield();
        continue;
      } else THROW_ERRNO("read()");
    } else return nread += ret; // TODO: return after the first read?
  }
  // TODO: need to make sure whole message is read even on slow connection
  // before this function returns
  // TODO: close socket if connection is too slow
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

uint32_t socket::addr() const {
  sockaddr_in addr;
  socklen_t addr_size = sizeof(addr);
  PCALL(getpeername)(fd, reinterpret_cast<sockaddr*>(&addr), &addr_size);
  static_assert(
    std::is_same_v<decltype(addr.sin_addr.s_addr),uint32_t>
  );
  return addr.sin_addr.s_addr;
}

} // end namespace ivan
