#ifndef IVAN_SERVER_HTTP_HH
#define IVAN_SERVER_HTTP_HH

#include "socket.hh"

namespace ivan {

class basic_server;

namespace http {

const char* status_code(int code);

[[noreturn]]
void error(socket, int code, const char* str);

struct request {
  char *method{}, *path{}, *protocol{};

  request(socket, char* buffer, size_t size);
  ~request() { }
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
