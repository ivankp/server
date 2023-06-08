#include "http.hh"

#include <algorithm>

#include "const_map.hh"
#include "numconv.hh"
#include "strings.hh"
#include "error.hh"

// #include "debug.hh"

namespace ivan {
namespace http {
namespace {

#define HTTP_STATUS_CODE(CODE,STR) \
  { CODE, "HTTP/1.1 " #CODE " " STR "\r\n" }

constexpr auto status_codes = make_const_map<int,std::string_view>({
  HTTP_STATUS_CODE(200,"OK"),
  HTTP_STATUS_CODE(400,"Bad Request"),
  HTTP_STATUS_CODE(401,"Unauthorized"),
  HTTP_STATUS_CODE(403,"Forbidden"),
  HTTP_STATUS_CODE(404,"Not Found"),
  HTTP_STATUS_CODE(405,"Method Not Allowed"), // Allow: GET, POST
  HTTP_STATUS_CODE(411,"Length Required"),
  HTTP_STATUS_CODE(413,"Payload Too Large"),
  HTTP_STATUS_CODE(500,"Internal Server Error"),
  HTTP_STATUS_CODE(501,"Not Implemented")
});

#undef HTTP_STATUS_CODE

}

std::string_view status_code(int code) { return status_codes[code]; }

std::string response(
  int code,
  std::string_view headers,
  std::string_view mime,
  std::string_view data
) {
  return cat(
    status_codes[code],
    "Content-Type: ", mime, "\r\n"
    "Content-Length: ", data.size(), "\r\n",
    headers, "\r\n",
    data
  );
}

std::string response(
  int code,
  std::string_view headers,
  std::string_view mime,
  size_t size
) {
  return cat(
    status_codes[code],
    "Content-Type: ", mime, "\r\n"
    "Content-Length: ", size, "\r\n",
    headers, "\r\n"
  );
}

std::string response(
  int code,
  std::string_view headers
) {
  return cat(
    status_codes[code],
    headers, "\r\n"
  );
}

[[noreturn]]
void throw_error(std::string&& resp, std::string&& err) {
  throw http::error(std::move(resp),std::move(err));
}

request::request(socket sock, char* buffer, size_t size) {
  const auto nread = sock.read(buffer,size);
  if (nread == 0) return;

  // std::cout << std::string_view(buffer,nread) << std::endl;

  const char* const end = buffer + nread;
  char *a=buffer, *b=a; // cursors

  // parse first line of http request ===============================
  method = a;
  bool d = false;
  for (;; ++b) {
    if (b == end) {
unexpected_end:
      throw_error(
        response(400),
        IVAN_ERROR_PREF "HTTP header: unexpected end"
      );
bad_header:
      throw_error(
        response(400),
        IVAN_ERROR_PREF "HTTP header: bad header"
      );
    }
    const char c = *b;
    if (c == '\r') {
      continue;
    } else if (c == '\n') { // line end
      if (!path || path == a) [[unlikely]] goto bad_header;
      if (!starts_with(a,"HTTP/")) [[unlikely]] {
        throw_error(
          response(400),
          IVAN_ERROR_PREF "HTTP header: not HTTP protocol"
        );
      }
      protocol = a;
      for (;;) { // skip intermediate spaces
        const char c = *--a;
        if (!(c == ' ' || c == '\t')) break;
      }
      a[1] = '\0';
      a = b+1;
      --b;
      if (*b == '\r') --b;
      for (;;) { // skip trailing spaces
        const char c = *b;
        if (!(c == ' ' || c == '\t')) break;
        --b;
      }
      b[1] = '\0';
      b = a;
      break;
    } else if (c == ' ' || c == '\t') { // delimeter
      if (!d) {
        d = true;
        if (!path) *b = '\0';
      }
    } else [[likely]] {
      if (d) {
        d = false;
        a = b;
        if (!path) path = a;
      }
    }
  }

  // parse http headers =============================================
  headers.reserve(16);
  char *key = nullptr;
  for (;;) {
    if (b == end) goto unexpected_end;
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
    headers.begin(), headers.end(),
    [](const auto& a, const auto& b){ return strcmp(a.first,b.first) < 0; }
  );
  if (b == end) return; // no body

  // store pointer to request body ==================================
  body = b;
  body_size = end - b;

  // NOTE: instead of automatically allocating for data that doesn't fit
  // in the buffer, check if body + body_size == buffer + buffer_size
  // in the worker function
  // TODO: how to determine if there's anything left to read?
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
  auto it = std::lower_bound(
    headers.begin(), end, name,
    [](const auto& x, auto name) -> bool { return strcmp(x.first,name) < 0; }
  );
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
  auto it = std::lower_bound(
    headers.begin(), end, name,
    [](const auto& x, auto name) -> bool { return x.first < name; }
  );
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
}
