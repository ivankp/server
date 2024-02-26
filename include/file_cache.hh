#ifndef IVAN_FILE_CACHE_HH
#define IVAN_FILE_CACHE_HH

#include <ctime>
#include <map>

#include "strings.hh"

namespace ivan {

template <typename T>
class cache {
public:
  struct value_type {
    std::timespec ts;
    T cached;
  };
  size_t cache_size = 0;

protected:
  std::map<std::string,value_type,std::less<void>> dict;
};

struct cached_file {
  struct cached_data_struct {
    char* data = nullptr;
    size_t size = 0;

    operator bool() const noexcept { return data; }
  };
  struct cached_data: cached_data_struct {
    cached_data() noexcept = default;
    ~cached_data(); // free(data);
    cached_data(const cached_data&) = delete;
    cached_data& operator=(const cached_data&) = delete;
    cached_data(cached_data_struct o): cached_data_struct(o) { }
    cached_data(cached_data&& o): cached_data_struct(o) {
      o.data = nullptr;
      o.size = 0;
    }
    cached_data& operator=(cached_data&& o) {
      this->~cached_data();
      data = o.data;
      size = o.size;
      o.data = nullptr;
      o.size = 0;
      return *this;
    }
  } raw, gz;

  enum { RAW = 1, GZ = 2 };
};

class file_cache: public cache<cached_file> {
public:
  using value_type = cache<cached_file>::value_type;
  const value_type& operator()(const char* name, int flags);
};


}

#endif
