#include "mime.hh"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

#include "file.hh"
#include "error.hh"

namespace ivan {

mime_dict::mime_dict(const char* filename) {
  const auto [m,size] = read_whole_file(filename,true);
  try {
    dict.reserve(8);

    const char* const end = m + size;
    char *a=m, *b; // cursors

    bool key = true;
    for (;;) {
      for (;;) { // skip leading spaces
        if (a == end) goto done;
        const char c = *a;
        if (c==' ' || c=='\t' || c=='\n' || c=='\r') ++a;
        else break;
      }
      b = a;
      if (key) { // key
        for (;;) {
          if (b == end) THROW_ERROR("unexpected file end");
          const char c = *b;
          const bool space = c==' ' || c=='\t';
          if (space || c==',' || c=='\n') {
            *b = '\0';
            dict.emplace_back(a,nullptr);
            a = ++b;
            if (space) key = false;
            break;
          } else ++b;
        }
      } else { // value
        for (;;) {
          // requires one more byte than file size allocated
          if (b == end || *b == '\n') {
            const char *const val = a;
            a = b + 1;
            for (;;) { // strip trailing spaces
              const char c = *--b;
              if (!(c==' ' || c=='\t' || c=='\r')) { ++b; break; }
            }
            *b = '\0';
            key = true;

            for (auto it = dict.data() + dict.size(); !(--it)->second; ) {
              it->second = val;
            }

            break;
          } else ++b;
        }
      }
    }

done:
    std::sort(
      dict.begin(), dict.end(),
      [](const auto& a, const auto& b){ return strcmp(a.first,b.first) < 0; }
    );

  } catch (...) {
    ::free(m);
    throw;
  }
  this->m = m;
}
mime_dict::~mime_dict() { ::free(m); }

const char* mime_dict::operator()(const char* key, const char* val) noexcept {
  const auto a = std::begin(dict);
  const auto b = std::end  (dict);
  auto it = std::lower_bound(
    a, b, key,
    [](const auto& x, const char* key){ return strcmp(x.first,key) < 0; }
  );
  return (it != b && !strcmp(it->first,key)) ? it->second : val;
}

}
