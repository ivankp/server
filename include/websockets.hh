#ifndef IVANP_WEBSOCKET_HH
#define IVANP_WEBSOCKET_HH

#include <shared_mutex>

#include "http.hh"
#include "index_dict.hh"

namespace ivanp {

class basic_server;

class server_websockets {
  index_dict<bool> wss;
  std::mutex mx;

protected:
  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  // bool event(int fd);

public:
  server_websockets(): wss(16) { }

  void add_websocket(int sock) {
    std::lock_guard lock(mx);
    wss[sock] = true;
  }
  void remove_websocket(int sock) {
    std::lock_guard lock(mx);
    wss[sock] = false;
  }
  bool is_websocket(int sock) {
    std::lock_guard lock(mx);
    return wss[sock];
  }
};

namespace websocket {

struct head {
  using type = uint8_t;

  type opcode: 4 = 0;
  type rsv   : 3 = 0;
  type fin   : 1 = 0;
  type len   : 7 = 0;
  type mask  : 1 = 0;

  enum : type {
    cont='\x0', text='\x1', bin='\x2', close='\x8', ping='\x9', pong='\xA'
  };
};

struct frame: head, std::string_view {
  uint16_t code() const noexcept;
};

void handshake(socket, const http::request& req);
frame parse_frame(char* buff, size_t size);
frame receive_frame(socket, char* buff, size_t size);
void send_frame(
  socket, char* buffer, size_t size,
  std::string_view message, head::type opcode = head::text
);

} // end namespace websocket
} // end namespace ivanp

#endif
