#include "file.hh"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdlib>
#include <stdexcept>

#include "error.hh"

namespace ivan {

file::file(const char* name): fd(PCALL(open)(name,O_RDONLY)) { }
file::~file() { ::close(fd); }

regular_file::regular_file(const char* name): file(name) {
  struct stat sb;
  PCALL(fstat)(fd,&sb);
  if (!S_ISREG(sb.st_mode)) THROW_ERROR_CAT(
    "\"", name, "\": not a regular file"
  );
  size = (size_t)sb.st_size;
}

std::pair<char*,size_t> read_whole_file(const char* name) {
  regular_file f(name);
  char* m = static_cast<char*>(::malloc(f.size));
  if (::read(f,m,f.size) < 0) {
    ::free(m);
    THROW_ERRNO_CAT("\"",name,"\": read()");
  }
  return { m, f.size };
}

std::pair<char*,size_t> read_whole_file(const char* name, bool null) {
  regular_file f(name);
  char* m = static_cast<char*>(::malloc(f.size+null));
  if (::read(f,m,f.size) < 0) {
    ::free(m);
    THROW_ERRNO_CAT("\"",name,"\": read()");
  }
  if (null) m[f.size] = '\0';
  return { m, f.size };
}

bool file_exists(const char* fname) {
  struct stat sb;
  return ::stat(fname,&sb) == 0;
}

void write_loop(int fd, const char* r, const char* end) {
  while (r<end) {
    r += PCALL(write)(fd,r,end-r);
  }
}

}
