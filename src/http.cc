#include "http.hh"
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
#include "whole_file.hh"

#include "debug.hh"

namespace ivanp::http {
namespace {

class mimes_dict {
  std::string s;
  std::vector<const char*> m;
public:
  mimes_dict(const char* filename): s(whole_file(filename)) {
    size_t nlines = 0;
    for (char c : s) if (c=='\n') ++nlines;
    if (s.back()!='\n') ++nlines;
    m.reserve(nlines);
    for (char *a=s.data(), *b; ; ) {
      a += strspn(a," \t\r\n"); // trim preceding blanks
      if (!*a) break; // end
      if (*a=='#') { // comment
        a += strcspn(a+1,"\n");
        continue;
      }
      b = a + strcspn(a," \t\r\n"); // end of key
      if (strchr("\r\n",*b)) ERROR(filename); // no value
      *b = '\0';
      char* v1 = ++b;
      b += strspn(b," \t"); // trim blanks before value
      if (strchr("\r\n",*b)) ERROR(filename); // no value
      char* v2 = b;
      b += strcspn(b,"\r\n"); // move to end of line
      while (strchr(" \t",*--b)); // trim trailing blanks
      if (!*++b) break; // end
      *b = '\0';
      ++b;
      if (v1!=v2) memmove(v1,v2,b-v2); // move value next to key
      m.push_back(a);
      a = b;
    }
    std::sort(m.begin(),m.end(),chars_less{});
  }
  auto begin() const noexcept { return m.begin(); }
  auto end  () const noexcept { return m.end  (); }
} const mimes_dict("config/mimes");

const std::unordered_map<int,const char*> status_codes_dict {
  {400,"HTTP/1.1 400 Bad Request\r\n\r\n"},
  {401,"HTTP/1.1 401 Unauthorized\r\n\r\n"},
  {403,"HTTP/1.1 403 Forbidden\r\n\r\n"},
  {404,"HTTP/1.1 404 Not Found\r\n\r\n"},
  {405,"HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, POST\r\n\r\n"},
  {411,"HTTP/1.1 411 Length Required\r\n\r\n"},
  {413,"HTTP/1.1 413 Payload Too Large\r\n\r\n"},
  {500,"HTTP/1.1 500 Internal Server Error\r\n\r\n"},
  {501,"HTTP/1.1 501 Not Implemented\r\n\r\n"}
};

}

const char* mimes(const char* ext) noexcept {
  const auto end = mimes_dict.end();
  const auto it = std::lower_bound(mimes_dict.begin(),end,ext,chars_less{});
  if (it==end || chars_less{}(ext,*it)) return nullptr;
  const char* s = *it;
  return s+strlen(s)+1;
}
const char* status_code(int code) noexcept {
  return status_codes_dict.at(code);
}

request::request(
  socket sock,
  char* buffer,
  size_t size,
  size_t max_size,
  bool split_get
) {
  const auto nread = sock.read(buffer,size);
  if (nread == 0) return;

  header.reserve(8);
  char *a=buffer, *b=a, *d;
  int nr=0, nn=0;
  for (size_t n=0; ; ++b, ++n) {
    if (n==size) HTTP_ERROR(400,"HTTP header: exceeded buffer length");
    char c = *b;
    if (c=='\r' || c=='\n') { // end of line
      *b = '\0';
      if (c=='\r') {
        if (nr==0) nr = 1;
        else HTTP_ERROR(400,"HTTP header: \\r not followed by \\n");
      } else {
        nr = 0;
        if (++nn == 2) { ++b; break; } // end of header

        // parse line
        if (a==buffer) { // first line -----------------------------------
          d = a;
          while (*d!=' ') {
            if (*d=='\0') HTTP_ERROR(400,"HTTP header: bad header");
            ++d;
          }
          *d = '\0';
          method = a;
          a = d+1;

          d = b;
          while (*d=='\0') --d;
          while (*d!=' ') {
            if (*d=='\0') HTTP_ERROR(400,"HTTP header: bad header");
            --d;
          }
          *d = '\0';
          if (*a!='/') HTTP_ERROR(400,
            "HTTP header: path doesn't start with /");
          path = ++a;
          if (split_get) {
            a = strchr(a,'?');
            if (a) {
              *a = '\0';
              get = a+1;
            }
          }
          protocol = d+1;
          if (strncmp(protocol,"HTTP/",5))
            HTTP_ERROR(400,"HTTP header: not HTTP protocol");
        } else { // header ------------------------------------------
          d = a;
          while (*d!=':') {
            if (*d=='\0') HTTP_ERROR(400,
              "HTTP header: field line without \':\'");
            ++d;
          }
          *d = '\0';
          ++d;
          while (*d==' ') ++d;
          header.emplace_back(a,d);
        }

        a = b+1; // beginning of next line
      }
    } else if (c<'\x20' || '\x7E'<c) {
      char buf[5];
      sprintf(buf,"0x%02X",c);
      HTTP_ERROR(400,"HTTP header: invalid character ",buf);
    } else {
      nn = 0;
    }
  }
  std::stable_sort(header.begin(),header.end(),
    [](const entry& a, const entry& b){
      return chars_less{}(a.name,b.name);
    });

  // get POST data --------------------------------------------------
  const size_t nread_data = nread-(b-buffer);

  if (nread < size) { // default buffer was enough
    // NOTE: allows any request to have data, as long as the data fits in the
    // buffer
    // TODO: maybe only POST should be allowed to have data
    data = { b, nread_data };
  } else { // need to allocated a larger buffer
    if (!strcmp(method,"POST")) { // is POST
      const auto h = (*this)["Content-Length"]; // get header
      if (h.size()!=1) {
        if (h.size()==0) {
          HTTP_ERROR(411,"POST request: missing Content-Length");
        } else {
          HTTP_ERROR(411,"POST request: more than one Content-Length");
        }
      } else {
        const size_t cl = atol(h.value());
        if (cl == nread_data) { // exact cl
          data = { b, nread_data };
        } else if (cl < nread_data) { // claimed cl too short
          HTTP_ERROR(413,"POST request: Content-Length < read length");
        } else if (cl > max_size) { // longer than max size
          HTTP_ERROR(413,"POST request: Content-Length > max buffer size");
        } else { // allocate more space
          memcpy((this->buffer = new char[cl]), b, nread_data);
          const size_t nread_data2 = sock.read(
            this->buffer+nread_data, cl-nread_data) + nread_data;
          if (nread_data2 != cl) {
            HTTP_ERROR(413,"POST request: Content-Length != read length");
          }
          data = { this->buffer, cl };
        }
      }
    } else {
      HTTP_ERROR(413,method," request: nread > buffer size");
    }
  }
}

request::fields request::operator[](const char* name) const noexcept {
  const auto end = header.end();
  auto it = std::lower_bound(header.begin(),end,name,
    [](const auto& x, auto name) -> bool {
      return strcmp(x.name,name) < 0;
    });
  if (it==end || strcmp(it->name,name)) return { };
  fields fs { &*it, 1 };
  while (++it!=end) {
    if (strcmp(it->name,name)) break;
    ++fs.n;
  }
  return fs;
}
request::fields request::operator[](std::string_view name) const noexcept {
  const auto end = header.end();
  auto it = std::lower_bound(header.begin(),end,name,
    [](const auto& x, auto name) -> bool {
      return x.name < name;
    });
  if (it==end || it->name != name) return { };
  fields fs { &*it, 1 };
  while (++it!=end) {
    if (it->name != name) break;
    ++fs.n;
  }
  return fs;
}

float request::fields::q(const char* x) const noexcept {
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

void validate_path(const char* path) {
  if (const char c = *path; c == '/' || c == '\\')
    HTTP_ERROR(404,"path \"",path,"\" starts with \'",c,'\'');
  if (const char c = path[strlen(path)-1]; c == '/' || c == '\\')
    HTTP_ERROR(404,"path \"",path,"\" ends with \'",c,'\'');
  for (const char* p=path; ; ++p) {
    if (const char c = *p) {
      // allow only - . / _ 09 AZ az
      if (!( ('-'<=c && c<='9') || ('A'<=c && c<='Z')
          || c=='_' || ('a'<=c && c<='z') )) {
        HTTP_ERROR(404,"path \"",path,"\" contains \'",c,'\'');
      } else if (c=='.' && (p==path || *(p-1)=='/')) { // disallow ..
        while (*++p=='.') { }
        if (*p=='/' || *p=='\0') {
          HTTP_ERROR(404,
            "path \"",path,"\" contains \"",
            std::string_view((p==path ? p : p-1),(*p ? p+1 : p)),'\"');
        } else --p;
      }
    } else break;
  }
}

std::string with_header(
  std::string_view mime,
  std::string_view data,
  std::string_view headers
) {
  char buf[21];
  sprintf(buf,"%zu",data.size());
  return cat(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: ", mime, "\r\n"
    "Content-Length: ", buf, "\r\n",
    headers, "\r\n",
    data);
}

// FUNCTIONS below are NOT finished

void send_str(
  socket sock,
  std::string_view data,
  bool gz,
  const char* mime,
  std::string_view headers
) {
  if ( mime) mime = mimes(mime);
  if (!mime) mime = "text/plain; charset=UTF-8";

  // TODO: implement gzip for strings

  sock << http::with_header(mime, data, headers);
}

void send_file(
  socket sock,
  const char* filename,
  bool gz,
  std::string_view headers
) {
  const char* ext = strrchr(filename,'.');
  ext = ext ? ext+1 : "";

  // TODO: implement file caching
  // TODO: implement gzip for files

  http::send_str(sock, whole_file(filename), gz, ext, headers);
}

} // end namespace ivanp::http
