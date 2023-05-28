#include "error.hh"

#include <stdexcept>
#include <cerrno>

namespace ivan {

[[noreturn]]
void exception_errno(std::string_view s) {
  throw std::runtime_error( ivan::cat( s, std::strerror(errno) ) );
}

}
