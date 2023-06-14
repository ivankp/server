#include "error.hh"
#include "http.hh"

// #include "debug.hh"

namespace ivan {

char* split_query(char* path) {
  char* q = strchr(path,'?');
  if (q) { *q = '\0'; ++q; }
  return q;
}

void validate_path(char* path) {
  // request parser doesn't allow empty paths
  // but doesn't check for \0 in path
  // split_query(path) with path == "?..." would make path empty
  if (!path || !path[0]) http::throw_error(
    http::response(400),
    IVAN_ERROR_PREF "empty path"
  );
  if (path[0] != '/') {
    http::throw_error(
      http::response(403),
      IVAN_ERROR_PREF "path doesn't start with /"
    );
  }

  char *a = path, *b = a;
  char prev, c = '\0';
  for (;;) {
    prev = c;
    c = *b;
    if (c == '\0') {
      *a = '\0';
      break;
    } else if (c == '/') {
      if (prev == '/') ++b;
    } else if (!( // allow only - . / _ 09 AZ az
         ('-'<=c && c<='9')
      || ('A'<=c && c<='Z')
      || ('a'<=c && c<='z')
      || c=='_'
    )) {
      http::throw_error(
        http::response(403),
        IVAN_ERROR_PREF "disallowed chars in path"
      );
    } else if (prev == '/' && c == '.') {
      const char* d = b+1;
      while (*d == '.') ++d;
      const char c = *d;
      if (c == '/' || c == '\0')
        http::throw_error(
          http::response(403),
          IVAN_ERROR_PREF "dots in path"
        );
    }
    *a = *b; ++a; ++b;
  }
  if (a - path > 3 && a[-1] == '/') {
    http::throw_error(
      http::response(403),
      IVAN_ERROR_PREF "path ends with /"
    );
  }
}

std::vector<std::pair<const char*,char*>>
parse_query(char* m) {
  std::vector<std::pair<const char*,char*>> dict;
  // if (!m || !*m) return dict;
  if (!m) return dict;
  while (*m == '&') ++m;
  if (!*m) return dict;
  dict.reserve(8);

  char *a=m, *b=m, *key = nullptr;
  for (;;) {
    const char c = *b;
    if (c == '=' && !key) {
      *b = '\0';
      key = a;
      a = ++b;
    } else if (c == '&' || c == '\0') {
      *b = '\0';
      if (!key) { key = a; a = nullptr; }
      dict.push_back({key,a});
      if (c == '\0') break;
      while (*++b == '&');
      if (*b == '\0') break;
      key = nullptr;
      a = b;
    } else {
      ++b;
    }
  }

  return dict;
}

std::vector<std::pair<const char*,std::vector<char*>>>
parse_query_plus(char* m) {
  using vals_t = std::vector<char*>;
  std::vector<std::pair<const char*,vals_t>> dict;
  if (!m) return dict;
  while (*m == '&') ++m;
  if (!*m) return dict;
  dict.reserve(8);

  char *a=m, *b=m;
  vals_t* vals = nullptr;
  for (;;) {
    const char c = *b;
    if (c == '=' && !vals) {
      *b = '\0';
      vals = &dict.emplace_back(
        std::piecewise_construct,
        std::make_tuple(a),
        std::make_tuple()
      ).second;
      a = ++b;
    } else if (c == '&' || c == '\0') {
      *b = '\0';
      if (vals) {
        vals->push_back(a);
        vals = nullptr;
      } else {
        dict.emplace_back(
          std::piecewise_construct,
          std::make_tuple(a),
          std::make_tuple()
        );
      }
      if (c == '\0') break;
      while (*++b == '&');
      if (*b == '\0') break;
      a = b;
    } else if (c == '+' && vals) {
      *b = '\0';
      vals->push_back(a);
      a = ++b;
    } else {
      ++b;
    }
  }

  return dict;
}

size_t percent_decode(char* s) {
  char *a=s, *b=s, c, x;
  for (;; *a = c, ++a, ++b) {
    c = *b;
    if (c == '%') {
percent:
      x = *++b;
      if ('0' <= x && x <= '9') { x -= '0'   ; } else
      if ('A' <= x && x <= 'F') { x -= 'A'-10; } else
      if ('a' <= x && x <= 'f') { x -= 'a'-10; } else
      {
        *a = '%'; ++a;
        goto not_hex;
      }
      c = x << 4;

      x = *++b;
      if ('0' <= x && x <= '9') { x -= '0'   ; } else
      if ('A' <= x && x <= 'F') { x -= 'A'-10; } else
      if ('a' <= x && x <= 'f') { x -= 'a'-10; } else
      {
        *a = '%'; ++a;
        *a = b[-1]; ++a;
not_hex:
        if (x == '%') goto percent;
        if (x == '\0') break;
        c = x;
        continue;
      }
      c += x;

    } else if (c == '\0') break;
  }
  *a = '\0';
  return a - s;
}

}
