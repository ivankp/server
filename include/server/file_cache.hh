#ifndef IVANP_SERVER_FILE_CACHE_HH
#define IVANP_SERVER_FILE_CACHE_HH

#include <string>
#include <string_view>
#include <unordered_map>
// #include <list>
#include <ctime>
#include <shared_mutex>
#include <functional>

#include "struct_swap.hh"

namespace ivanp {

class cache_view;

class server_file_cache {
  struct cached_file {
    char *data = nullptr, *zdata = nullptr;
    size_t size = 0, zsize = 0;
    time_t last_access_time = 0;

    ~cached_file();
  };

  struct hash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;
    size_t operator()(const char* str) const        { return hash_type{}(str); }
    size_t operator()(std::string_view str) const   { return hash_type{}(str); }
    size_t operator()(const std::string& str) const { return hash_type{}(str); }
  };

  // https://stackoverflow.com/a/2504317/2640636 // LRU
  std::unordered_map< std::string, cached_file, hash, std::equal_to<> > dict;
  std::shared_mutex mx_dict;

protected:
  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  bool event(int fd); // inotify

public:
  size_t max_total_size = 1 << 24; // 16 MB
  size_t max_file_size  = 1 << 20; //  1 MB

  cache_view cached_file(std::string_view name, bool gz=false);
};

class cache_view {
  const char* _data = nullptr;
  size_t _size = 0;
  const server_file_cache* _cache = nullptr;

  friend class server_file_cache;
  cache_view(const char* data, size_t size, const server_file_cache* cache)
  : _data(data), _size(size), _cache(cache) { }

public:
  ~cache_view();
  cache_view(const cache_view&) = delete;
  cache_view& operator=(const cache_view&) = delete;
  cache_view(cache_view&& o) noexcept
  : _data(o._data), _size(o._size), _cache(o._cache) {
    o._data  = nullptr;
    o._size  = 0;
    o._cache = nullptr;
  }
  cache_view& operator=(cache_view&& o) noexcept {
    struct_swap(*this,o);
    return *this;
  }

  const char* data() const noexcept { return _data; }
  const char* size() const noexcept { return _size; }
  operator std::string_view() const noexcept {
    return _data ? { _data, _size } : { };
  }
};

} // end namespace ivanp

#endif
