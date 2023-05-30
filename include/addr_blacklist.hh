#ifndef IVAN_SERVER_ADDR_BLACKLIST_HH
#define IVAN_SERVER_ADDR_BLACKLIST_HH

#include <cstdint>
#include <vector>

// #include "server.hh"

namespace ivan {

class basic_server;

namespace server_features {

class addr_blacklist {
  std::vector<uint32_t> blacklist;

public:
  const char* blacklist_filename = nullptr;

  void blacklist_addr(uint32_t addr);

protected:
  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  void init();
  bool accept(uint32_t addr);
};

}
}

#endif
