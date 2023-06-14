#ifndef IVAN_SERVER_HTTP_HH
#define IVAN_SERVER_HTTP_HH

#include <vector>
#include <utility>
#include <string>
#include <string_view>
#include <stdexcept>

#include "socket.hh"

namespace ivan {
namespace http {

std::string_view status_code(int code);

struct error: std::runtime_error {
  std::string resp;
  error(auto&& resp, auto&& err)
  : std::runtime_error(std::move(err)), resp(std::move(resp))
  { }
};

[[noreturn]]
void throw_error(std::string&& resp, std::string&& err);

std::string response(
  int code,
  std::string_view headers,
  std::string_view mime,
  std::string_view data
);
inline std::string response(
  std::string_view headers,
  std::string_view mime,
  std::string_view data
) { return response(200,headers,mime,data); }
inline std::string response(
  std::string_view mime,
  std::string_view data
) { return response(200,{},mime,data); }

std::string response(
  int code,
  std::string_view headers,
  std::string_view mime,
  size_t size
);
inline std::string response(
  std::string_view headers,
  std::string_view mime,
  size_t size
) { return response(200,headers,mime,size); }
inline std::string response(
  std::string_view mime,
  size_t size
) { return response(200,{},mime,size); }

std::string response(
  int code,
  std::string_view headers
);
inline std::string response(
  std::string_view headers
) { return response(200,headers); }
inline std::string response(
  int code
) { return response(code,{}); }

class request {
public:
  char *method{}, *path{}, *protocol{}, *body{};
  size_t body_size = 0;
  using header_t = std::pair<const char*,const char*>;
  std::vector<header_t> headers;

public:
  request(socket, char* buffer, size_t size);

  struct fields {
    const header_t* ptr { };
    unsigned n = 0;

    const header_t* begin() const noexcept { return ptr; }
    const header_t* end  () const noexcept { return ptr+n; }
    unsigned size() const noexcept { return n; }
    const char* name () const noexcept { return ptr->first; }
    const char* value() const noexcept { return ptr->second; }
    double q(const char*) const noexcept;

    operator bool() const noexcept { return n; }

    bool operator==(const char* val) const noexcept;
    bool operator==(std::string_view val) const noexcept;

    bool contain(const char* val) const noexcept;
    bool contain(std::string_view val) const noexcept;
  };

  fields operator[](const char* name) const noexcept;
  fields operator[](std::string_view name) const noexcept;

  operator bool() const noexcept { return method; }
};

}
}

#endif
