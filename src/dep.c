#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <fcntl.h>

#define STR1(x) #x
#define STR(x) STR1(x)

#define CAT1(a,b) a##b
#define CAT(a,b) CAT1(a,b)

#define ERROR_PREF __FILE__ ":" STR(__LINE__) ": "

#define ERR(WHAT, ...) { \
  const int e = errno; \
  fprintf( \
    stderr, \
    ERROR_PREF WHAT ": [%d] %s\n", ##__VA_ARGS__, e, strerror(e) \
  ); \
  exit(1); \
}

#define ISBLANK(c) \
  ( c == ' ' || c == '\t' || c == '\n' || c == '\r' )

bool only_quoted_headers = true;

char* file_buf;
size_t file_cap;

typedef struct {
  const char* str;
  size_t len;
} string;

typedef struct target_s {
  const char* name;
  struct target_s** deps;
  size_t num_deps;
} target;

target
  executables = { .name = "all" },
  objects = { },
  sources = { },
  headers = { };

size_t bit_ceil(size_t v) {
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  return ++v;
}

void append_target(target* t, target* parent) {
  const size_t n = parent->num_deps;
  if (n == 0) {
    parent->deps = malloc(sizeof(target*));
  } else {
    const size_t cap = bit_ceil(n);
    if (n == cap) {
      const size_t cap_bytes = cap * sizeof(target*);
      parent->deps = realloc(parent->deps, cap_bytes * 2);
    }
  }
  parent->deps[n] = t;
  ++parent->num_deps;
}

target* add_target(const char* name, target* parent, target* set) {
  target* t = NULL;
  if (set) {
    target** it = set->deps;
    target** const end = it + set->num_deps;
    for (; it != end; ++it) { // find in set
      target* d = *it;
      if (!strcmp(name, d->name)) {
        t = d;
        break;
      }
    }
  }
  if (!t) {
    t = calloc(1, sizeof(target));
    t->name = name;
    if (set) append_target(t, set);
  }
  append_target(t, parent);
  return t;
}

void print_targets(const target* t) {
  if (!t->num_deps) return;

  printf("%s:", t->name);

  target** dep = t->deps;
  target** const end = dep + t->num_deps;
  for (; dep != end; ++dep)
    printf(" %s", (*dep)->name);
  printf("\n");

  dep = t->deps;
  for (; dep != end; ++dep)
    print_targets(*dep);
}

void free_targets(target* t) {
  // TODO
}

static inline char* strskp(char* dest, const char* src) {
  return dest + strspn(dest, src);
}

const char* scan_code(char** bufp, bool* main) {
  bool new_line = true;
  for (char* a = *bufp; ; ++a) {
    switch (*a) {
      case '\n': new_line = true;
      case ' ':
      case '\t': break;
      case '\r': goto next_line;
      case '#':
        if (!new_line) goto next_line;
        a = strskp(a+1, " \t");
        if (strncmp(a, "include", 7)) goto next_line;
        a = strskp(a+8, " \t");
        char close;
        switch (*a) {
          case '"':
            close = '"';
            break;
          case '<':
            if (!only_quoted_headers) {
              close = '>';
              break;
            } else goto next_line;
          case '\n':
            new_line = true;
            goto next;
          default:
            goto next_line;
        }
        for (char* b = ++a; ; ++a) {
          const char c = *a;
          if (c == close) { // return included header
            /* printf("L%d: %lu %.12s\n", __LINE__, strlen(*bufp), *bufp); */
            *bufp = a+1;
            /* printf("L%d: %lu %.12s\n", __LINE__, strlen(*bufp), *bufp); */
            *a = '\0';
            /* printf("L%d: %lu %.12s\n", __LINE__, strlen(*bufp), *bufp); */
            return b;
          } else if (c == '\n') {
            new_line = true;
            goto next;
          } else if (c == '\r') {
            goto next_line;
          } else if (c == '\0') {
            goto end;
          }
        }
      case '"':
        for (;; ++a) {
          switch (*a) {
            case '"':
              new_line = false;
              goto next;
            case '\n':
              new_line = true;
              goto next;
            case '\r': goto next_line;
            case '\0': goto end;
          }
        }
      case '/':
        switch (a[1]) {
          case '/':
            a += 2;
            goto next_line;
          case '*':
            if ((a = strstr(a+2, "*/"))) {
              a += 1; // position a at last character of searched substring
              new_line = false;
              goto next;
            } else goto end;
        }
      case 'm':
        if (!main || *main || strncmp(a+1, "ain", 3)) break;
        char* m = a; // main
        a = strskp(a+4, " \t\r\n");
        if (*a == '(') {
          const char* const buf = *bufp;
          const char* const end = buf + 2;
          const char* b = a;
          while (b > end) { // int
            char c = *--b;
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
              if (
                m - b > 1 && // at least one space between
                !strncmp((b -= 2), "int", 3) &&
                ( b == buf || (
                  c = b[-1], c == ' ' || c == '\t' || c == '\r' || c == '\n'
                ) )
              ) {
                *main = true;
              }
              break;
            }
          }
        } else {
          // unskip blanks, because \n need to set new_line = true
          // if not a mart of main
          a = m + 4;
        }
        goto next;
      case '\0': goto end;
    }

next: continue;

next_line:
    /* printf("L%d: %lu %.12s\n", __LINE__, strlen(a), a); */
    if ((a = strchr(a, '\n'))) {
      new_line = true;
      goto next;
    } else goto end;
  }

end:
  return NULL;
}

void dir_loop(char* path, size_t len, size_t cap) {
  DIR* D = opendir(path);
  if (!D) ERR("opendir('%s')", path)

  char* path2 = path + len;
  *path2 = '/';
  ++path2;
  ++len;

  const size_t rem = cap - len - 1; // 1 for null

  for (struct dirent *d; (d = readdir(D)); ) {
    const char* const name2 = d->d_name;

    if (!name2[strspn(name2, ".")]) // . ..
      continue;

    const size_t name2_len = strlen(name2);
    if (rem < name2_len) {
      fprintf(stderr, ERROR_PREF "path is too long\n");
      exit(1);
    }

    const size_t len2 = len + name2_len;
    memcpy(path2, name2, name2_len+1);

    switch (d->d_type) {
      case DT_DIR:
        dir_loop(path, len2, cap);
        break;
      case DT_REG:
      case DT_LNK:
        /* printf("%s\n", path); */
        const char* ext = strrchr(name2, '.');
        if (!ext) break;
        ++ext;
        if (!(
          strcmp(ext, "c") &&
          strcmp(ext, "cc") &&
          strcmp(ext, "cpp") &&
          strcmp(ext, "cxx") &&
          strcmp(ext, "c++") &&
          strcmp(ext, "C")
        )) {
          printf("%s\n", path);

          const int fd = open(path, O_RDONLY);
          if (fd < 0) ERR("open('%s')", path)

          struct stat sb;
          if (fstat(fd, &sb) < 0) ERR("fstat('%s')", path)
          if (!S_ISREG(sb.st_mode)) ERR("'%s' is not a regular file", path)
          const size_t file_size = sb.st_size;
          if (file_size == 0) continue;

          if (file_cap <= file_size) {
            const size_t file_size_ceil = bit_ceil(file_size + 1);
            if (file_size_ceil > (1 << 24)) ERR("'%s' is too large", path)
            free(file_buf);
            file_buf = malloc((file_cap = file_size_ceil));
          }

          if (read(fd, file_buf, file_size) < 0) ERR("read('%s')", path)
          file_buf[file_size] = '\0';

          bool main = false;
          for (char* buf = file_buf;;) {
            /* printf("L%d: %lu %.12s\n", __LINE__, strlen(buf), buf); */
            const char* header = scan_code(&buf, &main);
            if (!header) break;

            printf("  %s\n", header);
          }

          if (main) {
            string stem;
            stem.str = strchr(path, '/');
            stem.len = len2
                     - (stem.str - path) // prefix (src)
                     - (name2_len - (ext - name2) + 1); // extension

            char* name;

            // add executable
            name = malloc(3 + stem.len + 1);
            target* exe = add_target(name, &executables, NULL);
            memcpy(name, "bin", 3); name += 3;
            memcpy(name, stem.str, stem.len); name += stem.len;
            *name = '\0';

            // add object
            name = malloc(6 + stem.len + 2 + 1);
            target* obj = add_target(name, exe, &objects);
            memcpy(name, ".build", 6); name += 6;
            memcpy(name, stem.str, stem.len); name += stem.len;
            memcpy(name, ".o", 3);

            // add source
            name = malloc(len2+1);
            target* src = add_target(name, obj, &sources);
            memcpy(name, path, len2+1);
          }

          close(fd);
        }
        break;
    }
  }

  closedir(D);
}

int main(int argc, char** argv) {
  file_buf = malloc((file_cap = 1 << 12));

  char path[1 << 10] = "src";

  dir_loop(path, strlen(path), sizeof(path));

  printf("\n");
  print_targets(&executables);

  free(file_buf);
}
