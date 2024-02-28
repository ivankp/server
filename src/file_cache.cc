#include "file_cache.hh"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cstdlib>

#include "file.hh"
#include "error.hh"

namespace ivan {

cached_file::cached_data::~cached_data() { if (data) free(data); }

const file_cache::value_type&
file_cache::operator()(const char* name, int flags) {
  ivan::file fd(name);

  struct stat sb;
  PCALL(fstat)(fd,&sb);
  if (!S_ISREG(sb.st_mode)) THROW_ERROR_CAT(
    "\"", name, "\": not a regular file"
  );
  // TODO: return maybe so file existance doesn't need to be tested twice?
  // otherwise need a catch exception

  auto it = dict.find(name);
  if (it == dict.end()) { // add file to cache
    it = dict.emplace(name, value_type{ sb.st_mtim, { } }).first;
  } else {
    auto& [ts,cached] = it->second;
    if ( // compare timestamps
      sb.st_mtim.tv_sec  == ts.tv_sec &&
      sb.st_mtim.tv_nsec == ts.tv_nsec
    ) {
      if (cached.raw) flags = flags & ~cached_file::RAW;
      if (cached.gz ) flags = flags & ~cached_file::GZ ;
    } else { // clear old data
      cache_size -= cached.raw.size;
      cache_size -= cached.gz .size;
      cached = { };
    }
  }

  auto& value = it->second;
  if (flags & (cached_file::RAW | cached_file::GZ)) {
    cached_file::cached_data local_raw;
    const cached_file::cached_data_struct* raw;

    if (value.cached.raw) { // valid raw data cached
      raw = &value.cached.raw;
    } else { // read raw file
      local_raw.size = (size_t)sb.st_size;
      local_raw.data = static_cast<char*>( ::malloc(local_raw.size) );
      if (::read(fd,local_raw.data,local_raw.size) < 0) {
        THROW_ERRNO_CAT("\"",name,"\": read()");
      }
      if (flags & cached_file::RAW) { // need raw
        raw = &(value.cached.raw = std::move(local_raw));
        cache_size += local_raw.size;
      } else {
        raw = &local_raw;
      }
    }

    if (flags & cached_file::GZ) { // need gz




      cache_size += value.cached.gz.size;
    }
  }

  return value;
}

}
