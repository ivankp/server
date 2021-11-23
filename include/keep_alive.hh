#ifndef IVANP_SERVER_KEEP_ALIVE_HH
#define IVANP_SERVER_KEEP_ALIVE_HH

#include <unordered_map>
#include <shared_mutex>

namespace ivanp {

class basic_server;

class server_keep_alive {
  // TODO: optimize dictionary implementation
  std::unordered_map<int,int> alive_t2s; // timer, socket
  std::unordered_map<int,int> alive_s2t; // socket, timer
  std::shared_mutex mx;

protected:
  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  bool event  (int t);
  void release(int s);

public:
  void keep_alive(int sock, int sec);
};

} // end namespace ivanp

#endif
