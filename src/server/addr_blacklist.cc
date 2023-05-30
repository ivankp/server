#include "server/addr_blacklist.hh"

#include <algorithm>

#ifdef SERVER_VERBOSE_BLACKLIST
#include <iostream>
#include "addr_ip4.hh"
#endif

// #include "debug.hh"

namespace ivan {
namespace server_features {

void addr_blacklist::init() {
  // if (!blacklist_filename) return;

  // TODO: read whole file, parse addresses, sort

  // blacklist_addr(0x01'00'00'7Fu);
}

bool addr_blacklist::accept(uint32_t addr) {
  const bool listed = std::binary_search(
    blacklist.begin(), blacklist.end(), addr
  );
#ifdef SERVER_VERBOSE_BLACKLIST
  if (listed) {
    std::cout << "Rejected connection from blacklisted address "
      << addr_ip4(addr) << std::endl;
  }
#endif
  return !listed;
}

void addr_blacklist::blacklist_addr(uint32_t addr) {
  blacklist.push_back(addr);
  // TODO: sort
}

}
}
