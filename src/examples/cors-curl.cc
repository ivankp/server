#include <iostream>
#include <charconv>

#include <unistd.h>
#include <sys/wait.h>

#include "server.hh"
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

  server<> server(server_port,epoll_nevents);
  cout << "Listening on port " << server_port <<'\n'<< endl;

  server(nthreads, thread_buffer_size,
  [](unique_socket sock, char* buffer, size_t buffer_size){
    // HTTP *********************************************************
    try {
      http::request req(sock, buffer, buffer_size);
      if (!req) return;

#ifndef NDEBUG
      cout << req.method << " /" << req.path << ' ' << req.protocol << '\n';
      for (const auto& [name, val]: req.header)
        cout << "\033[1m" << name << "\033[0m: " << val << '\n';
      cout << req.data << endl;
#endif

      const char* path = req.path;
      if (!strcmp(req.method,"GET")) { // ===========================
        if (*path=='\0') {
          sock <<
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 16\r\n"
            "\r\n"
            "no url specified";
        } else if ([p=path]() mutable -> bool {
          for (char c; (c = *p); ++p) {
            if (c<'a' || 'z'<c)
              return strncmp(p,"://",3);
          }
          return true;
        }()) {
          sock <<
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "\r\n"
            "invalid url";
        } else {
          // curl command arguments
          std::vector<std::string> args {
            "curl", "-is",
            "-H", "User-Agent:"
              " Mozilla/5.0 (Windows NT 10.0; WOW64)"
              " AppleWebKit/537.36 (KHTML, like Gecko)"
              " Chrome/54.0.2840.99 Safari/537.36"
          };

          // add headers from request
          for (const auto& [name, val]: req["Accept-Encoding"]) {
            args.emplace_back("-H");
            args.emplace_back(cat(name,": ",val));
          }

          // add path to curl arguments
          args.emplace_back(req.path);

          // print curl command
#ifndef NDEBUG
          for (bool first=true; const auto& arg : args) {
            const bool s = arg.find_first_of(" \t\n") != std::string::npos;
            if (first) first = false;
            else cout << ' ';
            if (s) cout << '\'';
            cout << arg;
            if (s) cout << '\'';
          }
          cout << std::flush;
#endif

          std::vector<char*> cargs;
          cargs.reserve(args.size()+1);
          std::transform(args.begin(), args.end(), std::back_inserter(cargs),
            [](auto& arg){ return arg.data(); });
          cargs.emplace_back(nullptr);

          // create pipes
          int pipe1[2], pipe2[2];
          PCALL(pipe)(pipe1);
          PCALL(pipe)(pipe2);

          // fork and exec
          const auto pid = PCALLR(fork)();
          if (pid == 0) { // this is the child process
            ::close(pipe1[1]); // close the write end
            ::close(pipe2[0]); // close the read end
            PCALL(dup2)(pipe1[0], STDIN_FILENO);
            PCALL(dup2)(pipe2[1], STDOUT_FILENO);
            PCALL(execvp)("curl",cargs.data());
            // child process is replaced by exec()
          }

          // this is the original process
          ::close(pipe1[0]); // close the read end
          ::close(pipe2[1]); // close the write end
          ::close(pipe1[1]); // send EOF

          std::string header; // collect response headers
          // size_t content_length = 0;
          for (ssize_t nread = 0;;) {
            const ssize_t nread1 = ::read(
              pipe2[0],
              buffer + nread,
              buffer_size - nread - 1 // 1 less for \0
            );
            if (nread1 <= 0) { // could not read from pipe
              int status;
              ::wait(&status);
              if (status) { // child return status
                const char* fmt = "%d";
                char sstatus[16], slen[4];
                if (WIFEXITED(status)) {
                  status = WEXITSTATUS(status);
                  fmt = "exit %d";
                } else if (WIFSIGNALED(status)) {
                  status = WTERMSIG(status);
                  fmt = "signal %d";
                } else if (WIFSTOPPED(status)) {
                  status = WSTOPSIG(status);
                  fmt = "stop %d";
                }
                sprintf(slen,"%d",sprintf(sstatus,fmt,status)+5);
                sock << cat(
                  "HTTP/1.1 500 Internal Server Error\r\n"
                  "Content-Type: text/plain\r\n"
                  "Content-Length: ", slen, "\r\n"
                  "\r\n"
                  "curl ",sstatus
                );
              }
              return;
            }
            // if (nread1 == 0) continue;
            nread += nread1;
            buffer[nread] = '\0';

            // edit header chunk
            const char *a = buffer, *b, *end = buffer + nread;
            while (a < end) { // remove unwanted headers
              b = strchr(a,'\n');
              if (!b) {
                if (size_t(nread) == buffer_size)
                  HTTP_ERROR(400,"header field exceeded buffer size");
                memmove(buffer,a,end-a);
                break; // read more
              }
              const auto line = rstrip({a,b});
              ++b;
              if (line.empty()) {
                header += "Access-Control-Allow-Origin: *\r\n\r\n";
                header.append(b,end);
                sock << header;

                // send the rest of the file
                // content_length -= end-b;
                // if (content_length) {
                //   sock.sendfile(pipe2[0],content_length,end-b);
                // }

                const size_t buflen = 1 << 20;
                char* buf = reinterpret_cast<char*>(::malloc(buflen));
                for (;;) {
                  const auto n = ::read(pipe2[0],buf,buflen);
                  if (n <= 0) break;
                  sock.write(buf,n);
                }
                ::free(buf);

                return;
              } else {
                const std::basic_string_view<char,ivanp::char_traits_i>
                  line_i({line.data(),line.size()});
                if (!(
                  line_i.starts_with("cross-origin") ||
                  line_i.starts_with("Access-Control")
                )) {
                  // if (line_i.starts_with("Content-Length: ")) {
                  //   std::from_chars(
                  //     line.data()+16,
                  //     line.data()+line.size(),
                  //     content_length
                  //   );
                  // }
                  (header += line) += "\r\n";
                }
                a = b;
              }
            }
          } // end read loop

        }
      } else {
        HTTP_ERROR(501,req.method," method not implemented");
      }
    } catch (const http::error& e) {
      sock << e;
      throw;
    } catch (...) {
      sock << http::status_code(500);
      throw;
    }
    // **************************************************************
  });

  server.loop();
}
