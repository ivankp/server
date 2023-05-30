#include "http.hh"

#include <algorithm>

#include "const_map.hh"
#include "numconv.hh"
#include "strings.hh"
#include "error.hh"

#include "debug.hh"

namespace ivan {
namespace http {
namespace {

#define HTTP_STATUS_CODE(CODE,V,STR) \
  { CODE, "HTTP/" V " " #CODE " " STR "\r\n\r\n" }

constexpr auto status_codes = make_const_map<int,std::string_view>({
  HTTP_STATUS_CODE(400,"1.1","Bad Request"),
  HTTP_STATUS_CODE(401,"1.1","Unauthorized"),
  HTTP_STATUS_CODE(403,"1.1","Forbidden"),
  HTTP_STATUS_CODE(404,"1.1","Not Found"),
  HTTP_STATUS_CODE(405,"1.1","Method Not Allowed"), // Allow: GET, POST
  HTTP_STATUS_CODE(411,"1.1","Length Required"),
  HTTP_STATUS_CODE(413,"1.1","Payload Too Large"),
  HTTP_STATUS_CODE(500,"1.1","Internal Server Error"),
  HTTP_STATUS_CODE(501,"1.1","Not Implemented")
});

#undef HTTP_STATUS_CODE

}

std::string_view status_code(int code) { return status_codes[code]; }

std::string form_response(
  std::string_view mime,
  std::string_view headers,
  std::string_view data
) {
  return cat(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: ", mime, "\r\n"
    "Content-Length: ", data.size(), "\r\n",
    headers, "\r\n",
    data
  );
}

[[noreturn]]
void throw_error(
  int code,
  std::string_view str_ex,
  std::string_view str_resp
) {
  std::string_view status_code = status_codes[code];
  throw http::error(
    std::string( str_ex ),
    str_resp.empty()
    ? std::string( status_code )
    : cat( ( status_code.remove_suffix(2), status_code ), str_resp )
  );
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
    if (f) {
      if (!last) {
        if (a[0] != '/') [[unlikely]]
          HTTP_ERROR(400,"HTTP header: path doesn't start with /");
        path = a+1;
      } else {
        if (!starts_with(a,"HTTP/")) [[unlikely]]
          HTTP_ERROR(400,"HTTP header: not HTTP protocol");
        protocol = a;
      }
    }
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
