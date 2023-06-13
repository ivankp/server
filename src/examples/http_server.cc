#include <iostream>
#include <iomanip>
#include <sstream>

#include "server.hh"
#include "http.hh"
#include "mime.hh"
#include "url.hh"
#include "addr_ip4.hh"
#include "request_log.hh"
#include "strings.hh"
#include "error.hh"
#include "debug.hh"

using namespace ivan;
using std::cout, std::endl;

int main(int argc, char* argv[]) {
  mime_dict mime("etc/mimes");
  static const char* html_mime = mime("html");
  static const char* txt_mime = mime("txt");

  server<> server;
  server.port = 8080;

  cout << "Listening on port " << server.port <<'\n'<< endl;

  request_log log("log.txt");

  server([&](unique_socket sock, char* buffer, size_t buffer_size){
    const auto addr = sock.addr();
  try {
    cout << "\033[32;1m"
      "socket " << sock
      << " from " << addr_ip4(addr)
      << "\033[0m" << endl;

    const http::request req(sock,buffer,buffer_size);
    if (!req) {
      log(sock.addr(),"empty");
      return;
    }

    // --------------------------------------------------------------
    TEST(req.method)
    TEST(req.path)
    TEST(req.protocol)

    const auto user_agent = req["User-Agent"];
    log(addr,cat(
      req.method,' ',req.path,' ',req.protocol,' ',
      user_agent ? user_agent.value() : "?"
    ));

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

    if (!strcmp(req.method,"GET") || !strcmp(req.method,"HEAD")) {
      const bool GET = req.method[0] == 'G';
      char* q = split_query(req.path);
      if (q) TEST(q)
      validate_path(req.path);
      char* path = req.path+1; // skip leading slash

      if (*path=='\0') {
        std::string_view html =
          "<html><body><p>Hello World!</p></body></html>";
        if (GET) {
          sock << http::response(html_mime, html);
        } else {
          sock << http::response(html_mime, html.size());
        }
      } else if (!strcmp(path,"headers")) {
        std::stringstream ss;
        ss << "<html><body><table>";
        for (const auto& [key,val] : req.headers) {
          ss << "<tr><td><pre>"
             << key
             << "</pre></td><td><pre>"
             << val
             << "</pre></td></tr>";
        }
        ss << "</table></body></html>";
        if (GET) {
          sock << http::response(html_mime, ss.str());
        } else {
          sock << http::response(html_mime, ss.str().size());
        }
      } else if (!strcmp(path,"query")) {
        const auto query = parse_query(q);

        std::stringstream ss;
        ss << "<html><body><table>\n";
        for (const auto& [key,val] : query) {
          ss << "<tr> <td><pre>" << key << "</pre></td>";
          if (val) {
            ss << " <td><pre>" << val << "</pre></td>";
          }
          ss << " </tr>\n";
        }
        ss << "</table></body></html>\n";
        if (GET) {
          sock << http::response(html_mime, ss.str());
        } else {
          sock << http::response(html_mime, ss.str().size());
        }
      } else if (!strcmp(path,"plus")) {
        const auto query = parse_query_plus(q);

        std::stringstream ss;
        ss << "<html><body><table>\n";
        for (const auto& [key,vals] : query) {
          ss << "<tr> <td><pre>" << key << "</pre></td>";
          for (const char* val : vals) {
            ss << " <td><pre>" << val << "</pre></td>";
          }
          ss << " </tr>\n";
        }
        ss << "</table></body></html>\n";
        if (GET) {
          sock << http::response(html_mime, ss.str());
        } else {
          sock << http::response(html_mime, ss.str().size());
        }
      } else {
        http::throw_error(
          http::response(404),
          cat(IVAN_ERROR_PREF "path = \"",req.path,'\"')
        );
      }
    } else {
      http::throw_error(
        http::response(405,"Allow: GET, HEAD\r\n"),
        cat(IVAN_ERROR_PREF "method = \"",req.method,'\"')
      );
    }
  } catch (const http::error& e) {
    sock << e.resp;
    log(addr,e.what());
    throw;
  } catch (const std::exception& e) {
    sock << http::status_code(500);
    log(addr,e.what());
    throw;
  } catch (...) {
    sock << http::status_code(500);
    throw;
  }
  });
}
