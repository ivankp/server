#include <iostream>

#include "server.hh"
#include "addr_blacklist.hh"
#include "addr_ip4.hh"
#include "debug.hh"

using namespace ivan;
using std::cout, std::endl;

int main(int argc, char* argv[]) {
  server<
    server_features::addr_blacklist
  > server;
  server.port = 8080;
  server.thread_buffer_size = 1 << 10; // 1 kB buffer for testing

  cout << "Listening on port " << server.port <<'\n'<< endl;

  server([](unique_socket sock, char* buffer, size_t buffer_size){
    cout << "\033[32;1m"
      "socket " << sock
      << " from " << addr_ip4(sock.addr())
      << "\033[0m" << endl;

    // TODO: how to make sure the whole message is received?
    for (;;) {
      const auto nread = sock.read(buffer,buffer_size);
      if (nread == 0) return;

      cout << std::string_view(buffer,nread) << endl;

      if (nread < buffer_size) break;
    }

    // TODO: src/socket.cc:25: read(): Bad file descriptor
    // TODO: src/socket.cc:67: getpeername(): Bad file descriptor

    // socket socket 5 from 5 from 127.0.0.1
    // looks like socket is being accepted multiple times
  });
}
