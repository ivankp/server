#include "request_log.hh"

#include <iostream>
#include <string_view>
#include <ctime>

#include "addr_ip4.hh"

namespace ivan {

void request_log::operator()(uint32_t addr, std::string_view entry) {
  const std::time_t now = std::time(nullptr);

  char buf[64];
  auto len = std::strftime(
    buf, sizeof buf,
    "%Y %b %e %a %H:%M:%S",
    std::localtime(&now)
  );

  if (!len) {
    buf[0] = '?'; buf[1] = '\0';
    len = 1;
  }

  f << addr_ip4(addr) << ' '
    << std::string_view(buf,len) << ' '
    << entry << std::endl;
}

}
