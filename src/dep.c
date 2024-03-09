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

char* file_buf;
size_t file_cap;

typedef struct {
  const char* str;
  size_t len;
} string;

typedef struct target_s {
  const char* name;
  struct target_s* deps;
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

target* add_target(target* t, const char* name) {
  const size_t n = t->num_deps;
  if (n == 0) {
    t->deps = calloc(1, sizeof(target));
  } else {
    const size_t cap = bit_ceil(n);
    if (n == cap) {
      const size_t cap_bytes = cap * sizeof(target);
      t->deps = realloc(t->deps, cap_bytes * 2);
      memset(t->deps + cap, 0, cap_bytes);
    }
  }
  t->deps[n].name = name;
  ++t->num_deps;
  return &t->deps[n];
}

void print_targets(const target* t) {
  if (!t->num_deps) return;

  printf("%s:", t->name);

  const target* dep = t->deps;
  const target* const end = dep + t->num_deps;
  for (; dep != end; ++dep)
    printf(" %s", dep->name);
  printf("\n");

  dep = t->deps;
  for (; dep != end; ++dep)
    print_targets(dep);
}

void free_targets(target* t) {
  // TODO
}

bool is_main(const char* buf) {
#define ISBLANK(c) \
  ( c == ' ' || c == '\t' || c == '\n' || c == '\r' )

#define PREFIX "int"
#define MARKER "main"

  const size_t nmarker = sizeof(MARKER) - 1;
  const size_t nprefix = sizeof(PREFIX) - 1;

  const char* const prefix_last = buf + (nprefix - 1);

  for (const char* a = buf;;) {
    const char* m = strstr(a, MARKER);
    if (!m) return false;

    // printf(" %.6s\n", m);
    a = m + nmarker;
    for (;;) {
      const char c = *a;
      if (!ISBLANK(c)) break;
      ++a;
    }
    if (*a != '(') continue;

    for (const char* b = m; b > prefix_last; ) {
      char c = *--b;
      if (!ISBLANK(c)) {
        // printf(" %.12s\n", b);
        if (
          m - b > 1 && // at least one space between
          !strncmp((b -= (nprefix - 1)), PREFIX, nprefix) &&
          ( b == buf || ( c = b[-1], ISBLANK(c) ) )
        )
          return true;
        else
          break;
      }
    }
  }

#undef MARKER
#undef PREFIX
#undef ISBLANK
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

          /* while (find_include(file_buf)) { */
          /* } */

          if (is_main(file_buf)) {
            string stem;
            stem.str = strchr(path, '/');
            stem.len = len2
                     - (stem.str - path) // prefix (src)
                     - (name2_len - (ext - name2) + 1); // extension

            char* name;

            // add executable
            name = malloc(3 + stem.len + 1);
            target* exe = add_target(&executables, name);
            memcpy(name, "bin", 3); name += 3;
            memcpy(name, stem.str, stem.len); name += stem.len;
            *name = '\0';

            // add object
            name = malloc(6 + stem.len + 2 + 1);
            target* obj = add_target(exe, name);
            memcpy(name, ".build", 6); name += 6;
            memcpy(name, stem.str, stem.len); name += stem.len;
            memcpy(name, ".o", 3);

            // add source
            name = malloc(len2+1);
            target* src = add_target(obj, name);
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

  print_targets(&executables);

  free(file_buf);
}
