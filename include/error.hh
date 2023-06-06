#ifndef IVAN_ERROR_HH
#define IVAN_ERROR_HH

#include "strings.hh"

namespace ivan {

[[noreturn]]
void exception(std::string_view);

[[noreturn]]
void exception_errno(std::string_view);

}

#define STR1(x) #x
#define STR(x) STR1(x)

#define IVAN_ERROR_PREF __FILE__ ":" STR(__LINE__) ": "

#ifdef THROW_ERROR
#error "THROW_ERROR macro already defined"
#endif
#define THROW_ERROR(MSG) \
  ivan::exception( IVAN_ERROR_PREF MSG ": " )

#ifdef THROW_ERROR_CAT
#error "THROW_ERROR_CAT macro already defined"
#endif
#define THROW_ERROR_CAT(...) \
  ivan::exception(ivan::cat( IVAN_ERROR_PREF __VA_ARGS__ ": " ))

#ifdef THROW_ERRNO
#error "THROW_ERRNO macro already defined"
#endif
#define THROW_ERRNO(MSG) \
  ivan::exception_errno( IVAN_ERROR_PREF MSG ": " )

#ifdef THROW_ERRNO_CAT
#error "THROW_ERRNO_CAT macro already defined"
#endif
#define THROW_ERRNO_CAT(...) \
  ivan::exception_errno(ivan::cat( IVAN_ERROR_PREF __VA_ARGS__ ": " ))

#ifdef PCALL
#error "PCALL macro already defined"
#endif
#define PCALL(F,...) [](auto&&... x){ \
  const auto r = ::F(x...); \
  if (r == -1) { THROW_ERRNO( STR(F) "()" ); } \
  return r; \
}

#endif
