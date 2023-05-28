#include <iostream>

#include "server.hh"
#include "server/addr_blacklist.hh"
#include "addr_ip4.hh"
#include "debug.hh"

using namespace ivan;
using std::cout, std::endl;

int main(int argc, char* argv[]) {
  server<
    addr_blacklist
  > server;
  server.port = 8080;
  cout << "Listening on port " << server.port <<'\n'<< endl;

  server([](ivan::socket sock, char* buffer, size_t buffer_size){
    try {
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

    } catch (...) {
      sock.close();
      throw;
    }
  });
}
