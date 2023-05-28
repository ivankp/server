#include "addr_ip4.hh"

// #include <charconv>
#include <cstdio>

namespace ivan {

addr_ip4_str::addr_ip4_str(addr_ip4 addr) noexcept {
  // char *p = str, *const end = p+sizeof(str);
  // p = std::to_chars( p, end, addr.b[0] ).ptr;
  // *p = '.'; ++p;
  // p = std::to_chars( p, end, addr.b[1] ).ptr;
  // *p = '.'; ++p;
  // p = std::to_chars( p, end, addr.b[2] ).ptr;
  // *p = '.'; ++p;
  // p = std::to_chars( p, end, addr.b[3] ).ptr;
  // *p = '\0';

  snprintf(str,sizeof(str),"%u.%u.%u.%u",
    addr.b[0],
    addr.b[1],
    addr.b[2],
    addr.b[3]
  );
}

}
