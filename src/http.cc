#include "http.hh"

#include <algorithm>
#include <stdexcept>

#include "const_map.hh"
#include "strings.hh"
#include "error.hh"

#include "debug.hh"

#define HTTP_ERROR(code,MSG) \
  http::error(sock, code, IVAN_ERROR_PREF MSG)
#define HTTP_ERROR_CAT(code,...) \
  http::error(sock, code, ivan::cat(IVAN_ERROR_PREF, __VA_ARGS__).c_str())

namespace ivan {
namespace http {
namespace {

constexpr auto status_codes = make_const_map<int,const char*>({
  {400,"HTTP/1.1 400 Bad Request\r\n\r\n"},
  {401,"HTTP/1.1 401 Unauthorized\r\n\r\n"},
  {403,"HTTP/1.1 403 Forbidden\r\n\r\n"},
  {404,"HTTP/1.1 404 Not Found\r\n\r\n"},
  {405,"HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n\r\n"},
  {411,"HTTP/1.1 411 Length Required\r\n\r\n"},
  {413,"HTTP/1.1 413 Payload Too Large\r\n\r\n"},
  {500,"HTTP/1.1 500 Internal Server Error\r\n\r\n"},
  {501,"HTTP/1.1 501 Not Implemented\r\n\r\n"}
});

}

const char* status_code(int code) { return status_codes[code]; }

[[noreturn]]
void error(socket sock, int code, const char* str) {
  sock << status_codes[code];
  throw std::runtime_error(str);
}

request::request(socket sock, char* buffer, size_t size) {
  const auto nread = sock.read(buffer,size);
  if (nread == 0) return;

  std::cout << std::string_view(buffer,nread) << std::endl;

  const char* const end = buffer + nread;
  char *a=buffer, *b=a; // cursors

  // parse first line of http request ===============================
  method = a;
  int f = 0;
  for (;;) {
    if (b == end) {
exceeded_buffer_length:
      HTTP_ERROR(400,"HTTP header: exceeded buffer length");
bad_header:
      HTTP_ERROR(400,"HTTP header: bad header");
    }
    const char c = *b;
    const bool space = c == ' ';
    if (!space && c != '\n') { // not a delimeter
      ++b;
      continue;
    }
    const bool last = f > 1;
    if (space == last) goto bad_header;
    if (f) (last ? protocol : path) = a;
    ++f;
    if (space) {
      *b = '\0';
      ++b;
      for (;;) {
        if (b == end) goto exceeded_buffer_length;
        if (*b != ' ') break;
      }
      a = b;
    } else { // line end
      b[-(b != a && b[-1]=='\r')] = '\0';
      if (++b == end) return;
      a = b;
      break;
    }
  }

  // parse http headers =============================================
  headers.reserve(16);
  char *key = nullptr;
  for (;;) {
    if (b == end) goto exceeded_buffer_length;
    const char c = *b;
    if (!key && c == ':') { // key
      // strip white space
      while (*a == ' ' || *a == '\t') ++a;
      if (a == b) goto bad_header;
      char* d = b-1;
      while (*d == ' ' || *d == '\t') --d;
      *++d = '\0';
      key = a;
      a = ++b;
    } else if (c == '\n') { // line end, value
      while (*a == ' ' || *a == '\t') ++a;
      char* d = b-1;
      if (d >= a && *d == '\r') --d;
      if (d < a) { ++b; break; } // blank line indicates end of headers
      while (*d == ' ' || *d == '\t') --d;
      d[1] = '\0';
      headers.push_back({key,a});
      key = nullptr;
      if (++b == end) break; // last header but no blank line
      a = b;
    } else [[likely]] { // not a delimeter
      ++b;
    }
  }
  std::stable_sort(
    headers.begin(),
    headers.end(),
    [](const auto& a, const auto& b){ return strcmp(a.first,b.first) < 0; }
  );
  if (b == end) return; // no body

  // parse request body =============================================
}

bool request::fields::operator==(const char* val) const noexcept {
  return n==1 && !strcmp(val,value());
}
bool request::fields::operator==(std::string_view val) const noexcept {
  return n==1 && val==value();
}

bool request::fields::contain(const char* val) const noexcept {
  return std::find_if(begin(),end(),
    [val](const header_t& x){ return !strcmp(val,x.second); });
}
bool request::fields::contain(std::string_view val) const noexcept {
  return std::find_if(begin(),end(),
    [val](const header_t& x){ return val==x.second; });
}

request::fields request::operator[](const char* name) const noexcept {
  const auto end = headers.end();
  auto it = std::lower_bound(headers.begin(),end,name,
    [](const auto& x, auto name) -> bool {
      return strcmp(x.first,name) < 0;
    });
  if (it==end || strcmp(it->first,name)) return { };
  fields fs { &*it, 1 };
  while (++it!=end) {
    if (strcmp(it->first,name)) break;
    ++fs.n;
  }
  return fs;
}
request::fields request::operator[](std::string_view name) const noexcept {
  const auto end = headers.end();
  auto it = std::lower_bound(headers.begin(),end,name,
    [](const auto& x, auto name) -> bool {
      return x.first < name;
    });
  if (it==end || it->first != name) return { };
  fields fs { &*it, 1 };
  while (++it!=end) {
    if (it->first != name) break;
    ++fs.n;
  }
  return fs;
}

double request::fields::q(const char* x) const noexcept {
  const char *b, *q;
  for (auto [name,a] : (*this)) {
    for (char c=*a; c!='\0'; c=*(a=b), ++a) {
      while ((c=*a)==' ' || c=='\t') ++a;
      b = a;
      q = nullptr;
      while ((c=*b)!='\0' && c!=',') {
        if (c==';') q = b;
        ++b;
      }
      const char* b2 = q ? q : b;
      if (b2 > a) {
        while ((c=*--b2)==' ' || c=='\t') { }
        ++b2;
        if (strncmp(a,x,b2-a)) continue;
        if (!q) return 1;
        ++q;
        while ((c=*q)==' ' || c=='\t') ++q;
        if (c!='q') return 1;
        ++q;
        while ((c=*q)==' ' || c=='\t') ++q;
        if (c!='=') return 1;
        ++q;
        while ((c=*q)==' ' || c=='\t') ++q;
        if (c=='\0' || c==',') return 1;
        return atof(q);
      }
    }
  }
  return 0;
}

}

namespace server_features {

void http::init() {
  // TODO: initialize mimes etc
}

}
}
