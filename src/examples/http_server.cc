#include <iostream>

#include "server.hh"
#include "http.hh"
#include "addr_ip4.hh"
#include "strings.hh"
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
  try {
    cout << "\033[32;1m"
      "socket " << sock
      << " from " << addr_ip4(sock.addr())
      << "\033[0m" << endl;

    const http::request req(sock,buffer,buffer_size);
    if (!req) return;

    // --------------------------------------------------------------
    TEST(req.method)
    TEST(req.path)
    TEST(req.protocol)

    for (const auto& [key,val] : req.headers) {
      cout << key << ": " << val << '\n';
    }
    cout << endl;

    const auto cookies = req["Cookie"];
    cout << "Cookies:\n";
    for (const auto& header : cookies)
      cout << header.second << '\n';
    cout << endl;
    // --------------------------------------------------------------

    const char* path = req.path;
    if (!strcmp(req.method,"GET")) { // ===========================
      if (*path=='\0') {
        sock << http::form_response(
          "text/html; charset=UTF-8", {},
          "<html><body><p>Hello World!</p></body></html>"
        );
      } else http::throw_error(404,cat("path = \"/",path,'\"'));
    } else {
      http::throw_error(405,
        cat("path = \"/",path,'\"'),
        "Allow: GET\r\n\r\n"
      );
    }
  } catch (const http::error& e) {
    sock << e;
    e.throw_base();
  } catch (...) {
    sock << http::status_code(500);
    throw;
  }
  });
}
