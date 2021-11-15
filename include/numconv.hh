#ifndef IVANP_NUMCONV_HH
#define IVANP_NUMCONV_HH

#include <charconv>
#include <string_view>
#include <type_traits>

namespace ivanp {

// TODO: signed may need an extra char
// TODO: floats
template <typename T> requires std::is_integral_v<T>
struct ntos {
  static constexpr unsigned char len = ((616*sizeof(T)) >> 8) + 1;
  unsigned char n;
  char s[len];
  ntos(T x) noexcept: n(std::to_chars(s,s+len,x).ptr-s) { }
  operator std::string_view() const noexcept { return { s, n }; }
};

template <typename T>
ntos(T x) -> ntos<T>;

};

#endif
