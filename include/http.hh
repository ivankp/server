#ifndef IVANP_HTTP_HH
#define IVANP_HTTP_HH

#include <vector>

#include "socket.hh"
#include "error.hh"

namespace ivanp::http {

const char* mimes(const char*) noexcept;
const char* status_code(int) noexcept;

class error: public ivanp::error {
  int code;
public:
  template <typename... T> [[ gnu::always_inline ]]
  error(int code, T&&... x)
  : ivanp::error(std::forward<T>(x)...), code(code) { }
  friend socket operator<<(socket fd, const error& e) {
    return fd << status_code(e.code);
  }
};

#define HTTP_ERROR(code,...) \
  throw ivanp::http::error(code, IVANP_ERROR_PREF, __VA_ARGS__)

struct request {
  const char *method { }, *path { }, *get { }, *protocol { };
  struct entry { const char *name, *value; };
  std::vector<entry> header;
  std::string_view data;

private:
  char* buffer { };

public:
  request(
    socket,
    char* buffer,
    size_t size,
    size_t max_size=0,
    bool split_get=true
  );
  ~request() { delete[] buffer; }

  struct fields {
    const entry* ptr { };
    unsigned n = 0;

    const entry* begin() const noexcept { return ptr; }
    const entry* end  () const noexcept { return ptr+n; }
    unsigned size() const noexcept { return n; }
    const char* value() const noexcept { return ptr->value; }
    const char* name() const noexcept { return ptr->name; }
    float q(const char*) const noexcept;
  };

  fields operator[](const char* name) const noexcept;
  fields operator[](std::string_view name) const noexcept;

  operator bool() const noexcept { return method; }
};

void validate_path(const char* path);

std::string header(
  std::string_view mime,
  size_t size,
  std::string_view headers={}
);

std::string with_header(
  std::string_view mime,
  std::string_view data,
  std::string_view headers={}
);

// TODO: send file
// TODO: send string
// TODO: parse GET data
// TODO: parse POST data

// FUNCTIONS below are NOT finished

void send_str(
  socket,
  std::string_view str,
  bool gz=false,
  const char* ext=nullptr,
  std::string_view headers={}
);

void send_file(
  socket,
  const char* filename,
  bool gz=false,
  std::string_view headers={},
  bool head_only=false
);

} // end namespace ivanp::http

#endif
