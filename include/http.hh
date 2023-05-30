#ifndef IVAN_SERVER_HTTP_HH
#define IVAN_SERVER_HTTP_HH

#include <vector>
#include <utility>

#include "socket.hh"

namespace ivan {

class basic_server;

namespace http {

const char* status_code(int code);

[[noreturn]]
void error(socket, int code, const char* str);

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
