#ifndef IVAN_SERVER_HTTP_HH
#define IVAN_SERVER_HTTP_HH

#include <vector>
#include <utility>
#include <string>
#include <stdexcept>

#include "socket.hh"

#define HTTP_ERROR(code,MSG) ivan::http::throw_error( \
  code, IVAN_ERROR_PREF MSG )

namespace ivan {

class basic_server;

namespace http {

std::string_view status_code(int code);

struct error: std::runtime_error {
  std::string resp;

  error(std::string ex, std::string resp)
  : std::runtime_error(std::move(ex)), resp(std::move(resp)) { }

  friend socket operator<<(socket sock, const error& e) {
    return sock << e.resp;
  }

  [[noreturn]]
  void throw_base() const {
    throw *static_cast<const std::runtime_error*>(this);
  }
};

[[noreturn]]
void throw_error(
  int code,
  std::string_view str_ex,
  std::string_view str_resp = { }
);

std::string form_response(
  std::string_view mime,
  std::string_view headers,
  std::string_view data
);

class request {
public:
  char *method{}, *path{}, *protocol{};
  using header_t = std::pair<const char*,const char*>;
  std::vector<header_t> headers;

private:
  char* buffer { };

public:
  request(socket, char* buffer, size_t size);
  ~request() { if (buffer) delete[] buffer; }

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

namespace server_features {

class http {
public:
  const char* mimes_file_name = "etc/mimes";

protected:
  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  void init();
};

}

}

#endif
