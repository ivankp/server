#ifndef IVANP_WEBSOCKET_HH
#define IVANP_WEBSOCKET_HH

#include <unordered_set>
#include <shared_mutex>

#include "http.hh"

namespace ivanp {

class basic_server;

class server_websockets {
  std::unordered_set<int> wss;
  std::shared_mutex mx;

protected:
  virtual basic_server* base() noexcept = 0;
  virtual const basic_server* base() const noexcept = 0;

  // bool event(int fd);

public:
  void add_websocket(int sock) { wss.insert(sock); }
  bool is_websocket(int sock) { return wss.contains(sock); }
};

namespace websocket {

struct head {
  using type = uint8_t;

  type opcode: 4 = 0;
  type rsv   : 3 = 0;
  type fin   : 1 = 1;
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
