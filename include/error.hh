#ifndef IVANP_ERROR_HH
#define IVANP_ERROR_HH

#include <stdexcept>
#include <cerrno>
#include "string.hh"

namespace ivanp {

struct error: std::runtime_error {
  using std::runtime_error::runtime_error;
  template <typename... T> [[ gnu::always_inline ]]
  error(T&&... x): std::runtime_error(cat(std::forward<T>(x)...)) { };
  [[ gnu::always_inline ]]
  error(const char* str): std::runtime_error(str) { };
};

}

#define IVANP_STR1(x) #x
#define IVANP_STR(x) IVANP_STR1(x)

#define IVANP_ERROR_PREF __FILE__ ":" IVANP_STR(__LINE__) ": "

#ifdef ERROR
#error "ERROR macro already defined"
#endif
#define ERROR(...) throw ivanp::error(IVANP_ERROR_PREF, __VA_ARGS__)

#ifdef ERROR1
#error "ERROR1 macro already defined"
#endif
#define ERROR1(S) throw ivanp::error(IVANP_ERROR_PREF S)

#ifdef THROW_ERRNO
#error "THROW_ERRNO macro already defined"
#endif
#define THROW_ERRNO(...) ERROR(__VA_ARGS__,": ",std::strerror(errno))

#ifdef PCALL
#error "PCALL macro already defined"
#endif
#define PCALL(F) [](auto&&... x){ \
  if (::F(x...) == -1) throw ivanp::error( \
    IVANP_ERROR_PREF IVANP_STR(F) "(): ", std::strerror(errno) ); \
}

#ifdef PCALLR
#error "PCALLR macro already defined"
#endif
#define PCALLR(F) [](auto&&... x){ \
  const auto r = ::F(x...); \
  if (r == -1) throw ivanp::error( \
    IVANP_ERROR_PREF IVANP_STR(F) "(): ", std::strerror(errno) ); \
  return r; \
}

#endif
