#include <iostream>

#include "server.hh"
#include "addr_ip4.hh"
#include "debug.hh"

#include <string.h>

using namespace ivan;
using std::cout, std::cerr, std::endl;

int main(int argc, char* argv[]) {
  server server;
  server.port = 8080;
  server.thread_buffer_size = 1 << 10; // 1 kB buffer for testing

  cerr << "Listening on port " << server.port <<'\n'<< endl;

  server([](unique_socket sock, char* buffer, size_t buffer_size){
    cerr << "\033[32;1m"
      "socket " << sock
      << " from " << addr_ip4(sock.addr())
      << "\033[0m" << endl;

    for (;;) {
      const size_t nread = sock.read(buffer, buffer_size);
      cout << nread << endl;
      if (nread == 0) return;

      cout << std::string_view(buffer, nread) << endl;

      if (nread < buffer_size) return;
    }
  });
}
