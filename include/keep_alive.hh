#ifndef IVANP_SERVER_KEEP_ALIVE_HH
#define IVANP_SERVER_KEEP_ALIVE_HH

#include <mutex>
#include "index_dict.hh"
#include "int_fd.hh"

namespace ivanp {

class basic_server;

class server_keep_alive {
  index_dict<int_fd> alive_t2s; // timer, socket
  index_dict<int_fd> alive_s2t; // socket, timer
  std::mutex mx;

protected:
  server_keep_alive(): alive_t2s(16), alive_s2t(16) { }

  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  bool event  (int t);
  void release(int s);

public:
  void keep_alive(int sock, int sec);
};

} // end namespace ivanp

#endif
