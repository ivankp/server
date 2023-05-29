#include "server/http.hh"

#include <stdexcept>

#include "strings.hh"
#include "error.hh"

#include "debug.hh"

#define HTTP_ERROR(code,MSG) \
  http_error(sock, code, IVAN_ERROR_PREF MSG)
#define HTTP_ERROR_CAT(code,...) \
  http_error(sock, code, ivan::cat(IVAN_ERROR_PREF, __VA_ARGS__).c_str())

namespace ivan {
namespace {

[[noreturn]]
void http_error(socket sock, int code, const char* str) {
  sock << "HTTP/1.1 400 Bad Request\r\n\r\n";
  throw std::runtime_error(str);
}

}

void http::init() {
  // TODO: initialize mimes etc
}

http::request::request(socket sock, char* buffer, size_t size) {
  const auto nread = sock.read(buffer,size);
  if (nread == 0) return;

  std::cout << std::string_view(buffer,nread) << std::endl;

  const char* const end = buffer + nread;
  char *a=buffer, *b=a; // cursors

  // parse first line of http request ===============================
  method = a;
  for (int f=0;;) {
    if (b == end) {
exceeded_buffer_length:
      HTTP_ERROR(400,"HTTP header: exceeded buffer length");
bad_header:
      HTTP_ERROR(400,"HTTP header: bad header");
    }
    const char c = *b;
    const bool space = c == ' ';
    if (!space && c != '\r' && c != '\n') { // not a delimeter
      ++b;
      continue;
    }
    const bool last = f > 1;
    if (space == last) goto bad_header;
    if (f) (last ? protocol : path) = a;
    ++f;
    *b = '\0';
    ++b;
    if (space) {
      for (;;) {
        if (b == end) goto exceeded_buffer_length;
        if (*b != ' ') break;
      }
      a = b;
    } else { // line end
      if (b == end) return;
      if (c == '\r' && *b == '\n') ++b;
      a = b;
      break;
    }
  }

  // parse http headers =============================================
  if (a != end) {
    TEST(a)
  }

  // parse request body =============================================
}

}
