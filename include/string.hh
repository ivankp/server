#ifndef IVANP_STRING_HH
#define IVANP_STRING_HH

#include <string>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <cctype>

namespace ivanp {

[[nodiscard]]
inline const char* cstr(const char* x) noexcept { return x; }
[[nodiscard]]
inline const char* cstr(const std::string& x) noexcept { return x.c_str(); }

namespace impl {

template <typename T>
[[nodiscard]]
[[gnu::always_inline]]
inline std::string_view to_string_view(const T& x) noexcept {
  if constexpr (std::is_same_v<T,char>)
    return { &x, 1 };
  else
    return x;
}

template <typename T>
inline constexpr bool is_sv = std::is_same_v<T,std::string_view>;

}

[[nodiscard]]
inline std::string cat() noexcept { return { }; }
[[nodiscard]]
inline std::string cat(std::string x) noexcept { return x; }
[[nodiscard]]
inline std::string cat(const char* x) noexcept { return x; }
[[nodiscard]]
inline std::string cat(char x) noexcept { return std::string(1,x); }
[[nodiscard]]
inline std::string cat(std::string_view x) noexcept { return std::string(x); }

template <typename... T>
[[nodiscard]]
[[gnu::always_inline]]
inline auto cat(T... x) -> std::enable_if_t<
  (sizeof...(x) > 1) && (impl::is_sv<T> && ...),
  std::string
> {
  std::string s((x.size() + ...),{});
  char* p = s.data();
  ( ( memcpy(p, x.data(), x.size()), p += x.size() ), ...);
  return s;
}

template <typename... T>
[[nodiscard]]
[[gnu::always_inline]]
inline auto cat(const T&... x) -> std::enable_if_t<
  (sizeof...(x) > 1) && !(impl::is_sv<T> && ...),
  std::string
> {
  return cat(impl::to_string_view(x)...);
}

struct chars_less {
  using is_transparent = void;
  bool operator()(const char* a, const char* b) const noexcept {
    return strcmp(a,b) < 0;
  }
  template <typename T> requires std::is_class_v<T>
  bool operator()(const T& a, const char* b) const noexcept {
    return a < b;
  }
  template <typename T> requires std::is_class_v<T>
  bool operator()(const char* a, const T& b) const noexcept {
    return a < b;
  }
};

struct char_traits_i: public std::char_traits<char> {
  static char to_upper(char c) { return std::toupper(c); }
  static bool eq(char a, char b) { return to_upper(a) == to_upper(b); }
  static bool lt(char a, char b) { return to_upper(a) <  to_upper(b); }
  static int compare(const char* s1, const char* s2, size_t n) {
    while (n--) {
      const char a = *s1, b = *s2;
      if ( to_upper(a) < to_upper(b) ) return -1;
      if ( to_upper(a) > to_upper(b) ) return  1;
      ++s1; ++s2;
    }
    return 0;
  }
  static const char* find(const char* s, size_t n, char c) {
    c = to_upper(c);
    while (n--) {
      if (to_upper(*s) == c) return s;
      ++s;
    }
    return nullptr;
  }
};

[[nodiscard]]
inline std::string_view strip(std::string_view s) {
  const auto *a = s.data();
  const auto *b = a + s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(*a))) ++a;
  while (--b > a && std::isspace(static_cast<unsigned char>(*b))) ;
  return { a, b+1 };
}
[[nodiscard]]
inline std::string_view lstrip(std::string_view s) {
  const auto *a = s.data();
  const auto *b = a + s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(*a))) ++a;
  return { a, b };
}
[[nodiscard]]
inline std::string_view rstrip(std::string_view s) {
  const auto *a = s.data();
  const auto *b = a + s.size();
  while (--b > a && std::isspace(static_cast<unsigned char>(*b))) ;
  return { a, b+1 };
}

inline void replace_first(
  std::string& str, std::string_view s1, std::string_view s2
) {
  const auto pos = str.find(s1);
  if (pos != std::string::npos) str.replace(pos,s1.size(),s2);
}
inline void replace_all(
  std::string& str, std::string_view s1, std::string_view s2
) {
  for (size_t p; (p = str.find(s1,p)) != std::string::npos; ) {
    str.replace(p, s1.size(), s2);
    p += s2.size();
  }
}

} // end namespace ivanp

#endif
