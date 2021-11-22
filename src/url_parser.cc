#include "url_parser.hh"

#include <cstring>

#include "error.hh"

namespace ivanp {

url_parser::url_parser(const char* url) {
  const char* p = strchr(url,'?');
  if (p) {
    path = percent_decode({url,p});
    ++p;
    const char* const end = p + strlen(p);
    while (p!=end) {
      const char* amp = strchr(p,'&');
      if (!amp) amp = end;
      else if (p==amp) { ++p; continue; }
      const char* eq = reinterpret_cast<const char*>(memchr(p,'=',amp-p));
      auto& vals = params[percent_decode({p,eq?eq:amp})];
      if (eq) {
        ++eq;
        for (;;) {
          const char* pl = reinterpret_cast<const char*>(memchr(eq,'+',amp-eq));
          vals.emplace_back(percent_decode({eq,pl?pl:amp}));
          if (!pl) break;
          eq = pl + 1;
        }
      }
      p = amp;
      if (p!=end) ++p;
    }
  } else {
    path = percent_decode(url);
  }
}

std::string percent_decode(std::string_view s) {
  const char* b = reinterpret_cast<const char*>(memchr(s.data(),'%',s.size()));
  if (!b) return std::string(s);
  std::string o;
  o.reserve(s.size());
  const char* a = s.data();
  const char* const end = a + s.size();
  for (;;) {
    if (end-b < 3) b = end;
    o.append(a,b);
    if (b==end) break;

    a = b+3;
    char c = '\0';
    while (++b < a) {
      c <<= 4;
      char x = *b;
      if ('0'<=x && x<='9') {
        c += x - '0';
      } else if ('A'<=x && x<='F') {
        c += x - 'A' + 10;
      } else if ('a'<=x && x<='f') {
        c += x - 'a' + 10;
      } else {
        a -= 3;
        break;
      }
    }
    if (a==b) o += c;
    b = reinterpret_cast<const char*>(memchr(b,'%',end-b));
    if (!b) b = end;
  }
  return o;
}

void validate_path(std::string_view path) {
  if (path.empty()) ERROR1("path is empty");
  const char* p = path.data();
  const char* const end = p + path.size();
  char c = *p;
  if (c == '/') ERROR("path \"",path,"\" starts with '/'");
  c = *(end-1);
  if (c == '/') ERROR("path \"",path,"\" ends with '/'");
  for (; p<end; ++p) {
    c  = *p;
    if (!( // allow only - . / _ 09 AZ az
         ('-'<=c && c<='9')
      || ('A'<=c && c<='Z')
      || ('a'<=c && c<='z')
      || c=='_'
    )) {
      ERROR("path \"",path,"\" contains \'",c,'\'');
    } else if (c=='.' && (p==path.data() || *(p-1)=='/')) { // disallow ..
      while (*++p=='.') { }
      if (p==end || *p=='/') ERROR("path \"",path,"\" contains dots");
      else --p;
    }
  }
}

}
