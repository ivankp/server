#include <iostream>

#include "server.hh"
#include "addr_ip4.hh"
#include "debug.hh"

#include <string.h>

using namespace ivan;
using std::cout, std::endl;

int main(int argc, char* argv[]) {
  server server;
  server.port = 8080;
  server.thread_buffer_size = 1 << 10; // 1 kB buffer for testing

  std::cerr << "Listening on port " << server.port <<'\n'<< endl;

  // TEST(EAGAIN) // 11

  server([](/*unique_*/socket sock, char* buffer, size_t buffer_size){
    std::cerr << "\033[32;1m"
      "socket " << sock
      << " from " << addr_ip4(sock.addr())
      << "\033[0m" << endl;

    // TODO: how to make sure the whole message is received?
    for (;;) {
      // const auto nread = sock.read(buffer,buffer_size);
      const auto nread = ::read(sock, buffer, buffer_size);
      TEST(nread)
      if (nread == 0) return;
      else if (nread < 0) {
        const int e = errno;
        TEST(e)
        TEST(strerror(e))
        return;
      }

      TEST(std::this_thread::get_id())
      cout << std::string_view(buffer,nread);
      cout << '\n';
      cout.flush();

      if (nread < buffer_size) {
        ::close(sock);
        return;
      }
    }

    // TODO: src/socket.cc:25: read(): Bad file descriptor
    // TODO: src/socket.cc:67: getpeername(): Bad file descriptor

    // socket socket 5 from 5 from 127.0.0.1
    // looks like socket is being accepted multiple times
  });
}
