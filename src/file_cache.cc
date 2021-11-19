#include "server/file_cache.hh"

namespace ivanp {

server_file_cache::cached_file::~cached_file() {
  ::free( data); // ok to free nullptr
  ::free(zdata);
}

cache_view server_file_cache::cached_file(std::string_view name, bool gz) {
  { std::shared_lock read_lock(mx_dict);
    const auto it = dict.find(name);
    if (it!=dict.end()) {
      const auto& cached = it->second;
      return { cached.data, cached.size, this };
    }
  }
}

cache_view::~cache_view() {
  if (_cache) {
    mx_file.unlock_shared();
  }
}
