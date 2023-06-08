#include "error.hh"
#include "http.hh"

// TODO: redo validation
// TODO: parse % encoding
// TODO: parse query params

namespace ivan {

char* split_query(char* path) {
  char* q = strchr(path,'?');
  if (q) { *q = '\0'; ++q; }
  return q;
}

void validate_path(char* path) {
  // request parser doesn't allow empty paths
  // but doesn't check for \0 in path
  // split_query(path) with path == "?a" would make path empty
  if (!path || !path[0]) http::throw_error(
    http::response(400),
    IVAN_ERROR_PREF "empty path"
  );

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
  if (path[0] != '/') {
    http::throw_error(
      http::response(403),
      IVAN_ERROR_PREF "path doesn't start with /"
    );
  }
  if (a - path > 3 && a[-1] == '/') {
    http::throw_error(
      http::response(403),
      IVAN_ERROR_PREF "path ends with /"
    );
  }
}

}
