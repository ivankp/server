#include <iostream>

#include "server.hh"
#include "keep_alive.hh"
#include "websockets.hh"
#include "http.hh"
#include "url_parser.hh"
#include "error.hh"
#include "numconv.hh"
#include "debug.hh"

using namespace ivanp;
using std::cout;
using std::endl;

int main(int argc, char* argv[]) {
  const port_t server_port = 12345;
  const unsigned nthreads = std::thread::hardware_concurrency();
  const unsigned epoll_nevents = 64;
  const size_t thread_buffer_size = 1<<13;

  server<
    server_keep_alive,
    server_websockets
  > server(server_port,epoll_nevents);
  cout << "Listening on port " << server_port <<'\n'<< endl;

  server(nthreads, thread_buffer_size,
  [&server](socket sock, char* buffer, size_t buffer_size){
    // TEST(std::this_thread::get_id())
    if (server.is_websocket(sock)) {
    // WebSocket ****************************************************
    try {
      namespace ws = ivanp::websocket;
      INFO("35;1","socket ",ntos((int)sock)," websocket");
      size_t len = 0;
      const char* p;
      for (;;) {
        auto frame = ws::receive_frame(sock,buffer+len,buffer_size-len);
        if (frame.opcode == ws::head::close) {
          ws::send_frame(sock,buffer,buffer_size, // close frame
            { frame.data(), 2 }, ws::head::close
          );
          server.remove_websocket(sock);
          sock.close();
          return;
        }
        if (frame.fin) {
          p = len ? buffer : frame.data();
          len += frame.size();
          break;
        } else {
          memmove(buffer+len,frame.data(),frame.size());
          len += frame.size();
          std::this_thread::yield();
        }
      }
      if (!len) return;
      // TEST(len)
      // for (size_t i=0; i<len; ++i) {
      //   cout << unsigned(p[i]) << '\n';
      // }
      // cout << endl;
      const char* brk = reinterpret_cast<const char*>(memchr(p,'\0',len));
      if (brk && p<brk && size_t(brk-p)!=len) {
        // p points inside the buffer
        // send_frame can invalidate p
        const std::string msg(p,len);
        server.every_websocket([&](int w){
          ws::send_frame(w,buffer,buffer_size,msg);
        });
      }

    } catch (...) {
      // namespace ws = ivanp::websocket;
      // ws::send_frame(sock,buffer,buffer_size, // close frame
      //   "\x03\xEA" /* 1002 */, ws::head::close
      // );
      // TODO: sending close message breaks
      server.remove_websocket(sock);
      sock.close();
      throw;
    }
    } else {
    // HTTP *********************************************************
    try {
      INFO("35;1","socket ",ntos((int)sock)," http");

      http::request req(sock, buffer, buffer_size);
      if (!req) return;

      bool keep_alive = atof(req.protocol+5) >= 1.1;
      for (const auto [name,val] : req["Connection"]) {
        if (!strcmp(val,"keep-alive")) keep_alive = true;
        else if (!strcmp(val,"close")) {
          keep_alive = false;
          break;
        }
      }

#ifndef NDEBUG
      cout << req.method << " /" << req.path << ' ' << req.protocol << '\n';
      for (const auto& [name, val]: req.header)
        cout << "\033[1m" << name << "\033[0m: " << val << '\n';
      cout << req.data << endl;
#endif

      const url_parser url(req.path);
      std::string_view path = url.path;

      if (!strcmp(req.method,"GET")) {
        if (path == "chat") { // upgrade to websocket
          websocket::handshake(sock, req, "http://localhost:12345");
          server.add_websocket(sock);
          return;
        } else { // send a file
send_file:
          try {
            if (path.empty()) path = "index.html";
            else validate_path(path); // disallow arbitrary paths
          } catch (const std::exception& e) {
            HTTP_ERROR(404,e.what());
          }
          bool gz = req["Accept-Encoding"].q("gzip");
          http::send_file(
            sock, cat("files/chat/",path).c_str(), gz, {},
            req.method[0]=='H'
          );
        }
      } else if (!strcmp(req.method,"HEAD")) {
        goto send_file;
      } else {
        HTTP_ERROR(501,req.method," method not implemented");
      }

      if (keep_alive) server.keep_alive(sock,5);
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
    }
    // **************************************************************
  });

  server.loop();
}
