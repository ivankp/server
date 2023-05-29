#ifndef IVAN_SERVER_HTTP_HH
#define IVAN_SERVER_HTTP_HH

#include "socket.hh"

namespace ivan {

class basic_server;

class http {

public:
  struct request {
    char *method{}, *path{}, *protocol{};

    request(socket, char* buffer, size_t size);
    ~request() { }
  };

protected:
  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  void init();
};

}

#endif
