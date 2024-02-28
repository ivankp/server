#include "numconv.hh"
#include "error.hh"

#include <stdexcept>
#include <cerrno>

namespace ivan {

[[noreturn]]
void exception(std::string_view s) {
  throw std::runtime_error(std::string( s ));
}

[[noreturn]]
void exception_errno(std::string_view s) {
  throw std::runtime_error( ivan::cat( s, errno, ": ", std::strerror(errno) ) );
}

}
