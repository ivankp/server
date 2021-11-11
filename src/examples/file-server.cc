#include <iostream>

#include "server.hh"
#include "server/keep_alive.hh"
#include "http.hh"
#include "error.hh"
#include "debug.hh"

using namespace ivanp;
using std::cout;
using std::endl;

int main(int argc, char* argv[]) {
  const port_t server_port = 8080;
  const unsigned nthreads = std::thread::hardware_concurrency();
  const unsigned epoll_nevents = 64;
  const size_t thread_buffer_size = 1<<13;

  server<
    server_keep_alive
  > server(server_port,epoll_nevents);
  cout << "Listening on port " << server_port <<'\n'<< endl;

  server(nthreads, thread_buffer_size,
  [&server](socket sock, char* buffer, size_t buffer_size){
    // HTTP *********************************************************
    try {
      server.keep_alive_release(sock); // remove keep-alive timer

      http::request req(sock, buffer, buffer_size);
      if (!req) return;
      INFO("35;1","HTTP");

      bool keep_alive = atof(req.protocol+5) >= 1.1;
      for (const auto [name,val] : req["Connection"]) {
        if (!strcmp(val,"keep-alive")) keep_alive = true;
        else if (!strcmp(val,"close")) {
          keep_alive = false;
          break;
        }
      }

#ifndef NDEBUG
      cout << req.method << " /" << req.path;
      if (req.get) cout << '?' << req.get;
      cout << ' ' << req.protocol << '\n';
      for (const auto& [name, val]: req.header)
        cout << "\033[1m" << name << "\033[0m: " << val << '\n';
      cout << req.data << endl;
#endif

      const char* path = req.path;
      if (!strcmp(req.method,"GET")) { // ===========================
        bool gz = req["Accept-Encoding"].q("gzip");
        if (*path=='\0') {
        } else { // serve any allowed file --------------------------
          http::validate_path(path); // disallow arbitrary paths
          http::send_file(sock, cat("files/",path).c_str(), gz);
        }
      } else if (!strcmp(req.method,"HEAD")) { // ===================
      } else {
        HTTP_ERROR(501,req.method," method not implemented");
      }

      // https://en.wikipedia.org/wiki/HTTP_persistent_connection
      if (keep_alive) server.keep_alive(sock,10);
      else sock.close();
      // must close socket on error
    } catch (const http::error& e) {
      sock << e;
      sock.close();
      throw;
    } catch (...) {
      sock << http::status_code(500);
      sock.close();
      throw;
    }
    // **************************************************************
  });

  server.loop();
}
