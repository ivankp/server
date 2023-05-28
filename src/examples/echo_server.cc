#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>

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
      sockaddr_in addr;
      socklen_t addr_size = sizeof(addr);
      ::getpeername(sock, reinterpret_cast<sockaddr*>(&addr), &addr_size);

      const addr_ip4 ip = addr.sin_addr.s_addr;

      cout << "\033[32;1m"
        "socket " << sock
        << " from " << ip
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
