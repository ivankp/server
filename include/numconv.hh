#ifndef IVANP_NUMCONV_HH
#define IVANP_NUMCONV_HH

#include <charconv>
#include <string_view>
#include <type_traits>

namespace ivanp {

template <typename T>
struct ntos;

template <typename T>
requires std::is_integral_v<T>
      || std::is_same_v<T,double>
      || std::is_same_v<T,float>
struct ntos<T> {
  unsigned char n;
  char s[
    std::is_integral_v<T>
    ? ((616*sizeof(T)) >> 8) + 1 + std::is_signed_v<T>
    : std::is_same_v<T,double> ? 24 : 16
  ];
  ntos(T x) noexcept: n(std::to_chars(s,s+sizeof(s),x).ptr-s) { }
  operator std::string_view() const noexcept { return { s, n }; }
};

template <>
struct ntos<bool> {
  bool x;
  ntos(bool x) noexcept: x(x) { }
  operator std::string_view() const noexcept { return x ? "true" : "false"; }
};

template <typename T>
ntos(T x) -> ntos<T>;

}

#endif
