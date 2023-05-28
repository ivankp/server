#ifndef IVAN_ADDR_IP4_HH
#define IVAN_ADDR_IP4_HH

#include <cstdint>

namespace ivan {

struct addr_ip4;

struct addr_ip4_str {
  char str[16];
  addr_ip4_str(addr_ip4 addr) noexcept;
};

struct addr_ip4 {
  union {
    uint32_t u;
    uint8_t b[4];
  };
  constexpr addr_ip4(uint32_t u) noexcept: u(u) { }
  constexpr addr_ip4(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) noexcept
  : b { b0, b1, b2, b3 } { }

  addr_ip4_str str() const noexcept { return *this; }
};
constexpr bool operator==(addr_ip4 a, addr_ip4 b) noexcept { return a.u == b.u; }
constexpr bool operator!=(addr_ip4 a, addr_ip4 b) noexcept { return a.u != b.u; }

}

#include <ostream>

inline std::ostream& operator<<(std::ostream& o, ivan::addr_ip4 addr) {
  return o << addr.str().str;
}

#endif
