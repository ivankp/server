#include <iostream>

#include "server.hh"
#include "http.hh"
#include "addr_ip4.hh"
#include "debug.hh"

using namespace ivan;
using std::cout, std::endl;

int main(int argc, char* argv[]) {
  server<
    server_features::http
  > server;
  server.port = 8080;

  cout << "Listening on port " << server.port <<'\n'<< endl;

  server([&server](unique_socket sock, char* buffer, size_t buffer_size){
    cout << "\033[32;1m"
      "socket " << sock
      << " from " << addr_ip4(sock.addr())
      << "\033[0m" << endl;

    const http::request req(sock,buffer,buffer_size);
    TEST(req.method)
    TEST(req.path)
    TEST(req.protocol)

    for (const auto& [key,val] : req.headers) {
      cout << key << ": " << val << '\n';
    }
    cout << endl;
  });
}
