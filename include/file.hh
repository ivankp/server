#ifndef IVAN_FILE_HH
#define IVAN_FILE_HH

#include <utility>
#include <cstddef>

namespace ivan {

struct file {
  int fd;
  file(const char* name);
  ~file();
  operator int() const noexcept { return fd; }
};

struct regular_file: file {
  size_t size;
  regular_file(const char* name);
};

std::pair<char*,size_t> read_whole_file(const char* name);
std::pair<char*,size_t> read_whole_file(const char* name, bool null);

bool file_exists(const char* fname);

void write_loop(int fd, const char* r, const char* end);

}

#endif
