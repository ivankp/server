#ifndef IVANP_LOCAL_FD_HH
#define IVANP_LOCAL_FD_HH

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "error.hh"

namespace ivanp {

struct local_fd {
  int fd;
  local_fd(const char* name) {
    if (!name || !*name) ERROR1("empty file name");
    fd = ::open(name,O_RDONLY);
    if (fd < 0) THROW_ERRNO("open(",name,")");
  }
  ~local_fd() { ::close(fd); }
  operator int() const noexcept { return fd; }
};

}

#endif
