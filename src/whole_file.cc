#include "whole_file.hh"
#include "local_fd.hh"

namespace ivanp {

std::string whole_file(const char* name) {
  local_fd fd(name);
  struct stat sb;
  if (::fstat(fd,&sb) < 0)
    THROW_ERRNO("fstat(",name,")");
  if (!S_ISREG(sb.st_mode))
    THROW_ERRNO("\"",name,"\" is not a regular file"); // TODO: no errno
  std::string m(sb.st_size,{});
  if (::read(fd,m.data(),m.size()) < 0)
    THROW_ERRNO("read(",name,")");
  return m;
}

}
