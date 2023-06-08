#ifndef IVAN_REQUEST_LOG_HH
#define IVAN_REQUEST_LOG_HH

#include <cstdint>
#include <string_view>
#include <fstream>

namespace ivan {

class request_log {
  std::ofstream f;

public:
  request_log(const char* filename): f(filename) { }
  void operator()(uint32_t addr, std::string_view entry);
};

}

#endif
