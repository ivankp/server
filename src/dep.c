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
    ERROR_PREF WHAT ": [%d] %s", ##__VA_ARGS__, e, strerror(e) \
  ); \
  exit(1); \
}

typedef struct target_s {
  char* name;
  struct target_s* deps;
  size_t num_deps;
} target;

target all = {
  .name = "all",
  .deps = NULL,
  .num_deps = 0
};

void print_target(const target* t) {
  if (!t->num_deps) return;

  printf("%s:", t->name);

  const target* dep = t->deps;
  const target* const end = dep + t->num_deps;
  for (; dep != end; ++dep)
    printf(" %s", dep->name);
  printf("\n");

  dep = t->deps;
  for (; dep != end; ++dep)
    print_target(dep);
}

void dir_loop(char* path, size_t len, size_t cap) {
  DIR* D = opendir(path);
  if (!D) ERR("opendir()")

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
          // TODO: dynamically extend srcs array
          /* memcpy( (srcs[num_srcs++] = malloc(len2+1)), path, len2+1 ); */

          printf("%s\n", path);

          static char* buf = NULL;
          static size_t cap = 0;

          const int fd = open(path, O_RDONLY);
          if (fd < 0) ERR("open('%s')", path)

          struct stat sb;
          if (fstat(fd, &sb) < 0) ERR("fstat('%s')", path)
          if (!S_ISREG(sb.st_mode)) ERR("'%s' is not a regular file", path)
          const size_t size = sb.st_size;

          if (cap < size) {
            if (size > (1 << 20)) ERR("'%s' is too large", path)
            cap = size < (1 << 12) ? (1 << 12) : size + 1;
            free(buf);
            buf = malloc(cap);
          }

          if (read(fd, buf, size) < 0) ERR("read('%s')", path)
          buf[size] = '\0';

          for (const char* a = buf;;) {
            const char* main = strstr(a, "main");
            if (!main) break;

#define ISBLANK(c) \
  ( c == ' ' || c == '\t' || c == '\n' || c == '\r' )

            // printf(" %.6s\n", main);
            a = main + 4; // length of main
            for (;; ++a) {
              const char c = *a;
              if (!ISBLANK(c)) break;
            }
            if (*a != '(')
              goto not_main;

            const char* b = main;
            for (const char* const end = buf - 2; b > end; ) {
              char c = *--b;
              if (!ISBLANK(c)) {
                b -= 2;
                // printf(" %.12s\n", b);
                if (!(
                  b < main-3 &&
                  !strncmp(b, "int", 3) &&
                  ( b == buf || ( c = b[-1], ISBLANK(c) ) )
                ))
                  goto not_main;
                else
                  break;
              }
            }

#undef ISBLANK

            printf("  main()\n");
            break;

not_main: ;
          }

          close(fd);
        }
        break;
    }
  }

  closedir(D);
}

int main(int argc, char** argv) {
  char path[1 << 10] = "src";

  dir_loop(path, strlen(path), sizeof(path));

  // for (char** p = srcs; *p; ++p) {
  //   printf("%s\n", *p);
  //   free(*p);
  // }
}
