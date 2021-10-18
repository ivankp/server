#include <iostream>

#include "server.hh"
#include "http.hh"
#include "error.hh"
#include "debug.hh"

using namespace ivanp;
using std::cout;
using std::endl;

int main(int argc, char* argv[]) {
  const server::port_t server_port = 8080;
  const unsigned nthreads = std::thread::hardware_concurrency();
  const unsigned epoll_nevents = 64;
  const size_t thread_buffer_size = 1<<13;

  server server(server_port,epoll_nevents);
  cout << "Listening on port " << server_port <<'\n'<< endl;

  server(nthreads, thread_buffer_size,
  [](socket sock, char* buffer, size_t buffer_size){
    // HTTP *********************************************************
    try {
      http::request req(sock, buffer, buffer_size, 0, false);
      if (!req) return;
      INFO("35;1","HTTP");

      bool keep_alive = atof(req.protocol+5) >= 1.1;
      for (const auto [name,val] : req["Connection"])
        keep_alive = !strcmp(val,"keep-alive");

#ifndef NDEBUG
      cout << req.method << " /" << req.path << ' ' << req.protocol << '\n';
      for (const auto& [name, val]: req.header)
        cout << "\033[1m" << name << "\033[0m: " << val << '\n';
      cout << req.data << endl;
#endif

      const char* path = req.path;
      if (!strcmp(req.method,"GET")) { // ===========================
        if (*path=='\0') {
          sock << cat(
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 16\r\n"
            "\r\n"
            "no url specified");
        } else if ([p=path]() mutable -> bool {
          for (char c; (c = *p); ++p) {
            if (c<'a' || 'z'<c)
              return strncmp(p,"://",3);
          }
          return true;
        }()) {
          sock << cat(
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
            "invalid url");
        } else {
          std::string sbuf;
          bool first = true;
          for (const auto& [name, val]: req["Accept-Encoding"]) {
            auto tmp = cat(" -H '",name,": ",val,'\'');
            if (first) {
              sbuf = std::move(tmp);
              first = false;
            } else {
              sbuf += tmp;
            }
          }
          auto cmd = cat(
            "curl -is", sbuf,
            " -H 'User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64)"
              " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.99 Safari/537.36'"
            " '", req.path, '\''
          );
          sbuf.clear();
          cout << cmd << endl;
          FILE* pipe = popen(cmd.c_str(),"r");
          if (!pipe) THROW_ERRNO("popen()");
          // TODO: pipe directly to socket instead of sbuf
          for (size_t n; (n = fread(buffer, 1, buffer_size, pipe)); )
            sbuf.append(buffer, n);
          int status = pclose(pipe);
          if (WIFEXITED(status) && (status = WEXITSTATUS(status))) {
            char sstatus[4], slen[4];
            sprintf(slen,"%d",sprintf(sstatus,"%d",status)+17);
            sock << cat(
              "HTTP/1.1 500 Internal Server Error\r\n"
              "Content-Type: text/plain\r\n"
              "Content-Length: ", slen, "\r\n"
              "\r\n"
              "curl exit status ",sstatus);
          } else {
            std::string header;
            header.reserve(1<<12);
            const char *a = sbuf.data(), *b;
            while ((b = strchr(a,'\n'))) { // remove unwanted headers
              std::basic_string_view<char,ivanp::char_traits_i> line(a,b);
              a = b+1;
              if (line.starts_with("cross-origin") ||
                  line.starts_with("Access-Control")
              ) continue;
              (header += rstrip({line.data(),line.size()})) += "\r\n";
              if (*a=='\r') ++a;
              if (*a=='\n') { ++a; break; }
            }
            header += "Access-Control-Allow-Origin: *\r\n\r\n";

            sbuf.replace(0,a-sbuf.data(),header);

            sock << sbuf;
          }
        }
      } else if (!strcmp(req.method,"POST")) { // ===================
        TEST(req.method)
      } else if (!strcmp(req.method,"HEAD")) { // ===================
        TEST(req.method)
      } else {
        HTTP_ERROR(501,req.method," method not implemented");
      }

      // must close the socket if not keep-alive
      if (!keep_alive) sock.close();
      // must close socket on any error
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