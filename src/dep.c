#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

#define TEST(FMT, var) \
  printf("L%4d: " FMT "\n", __LINE__, (var));

#define TEST_STR(str) \
  printf("L%4d: [%3lu] %.16s\n", __LINE__, strlen(str), str);

#define ERROR_PREF __FILE__ ":" STR(__LINE__) ": "

#define ERR(WHAT, ...) { \
  const int e = errno; \
  fprintf( \
    stderr, \
    ERROR_PREF WHAT ": [%d] %s\n", ##__VA_ARGS__, e, strerror(e) \
  ); \
  exit(1); \
}

// ------------------------------------------------------------------

uint32_t bit_ceil_32(uint32_t v) {
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return ++v;
}

uint64_t bit_ceil_64(uint64_t v) {
  --v;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v |= v >> 32;
  return ++v;
}

size_t lower_bound(
  void* ptr, size_t count, size_t size, const void* value,
  int (*comp)(const void*, const void*)
) {
  unsigned char *a = ptr, *b;
  size_t step;

  while (count > 0) {
    b = a;
    step = count / 2;
    b += step * size;

    if (comp(b, value) < 0) {
      a = b + size;
      count -= step + 1;
    } else {
      count = step;
    }
  }

  return (a - ((unsigned char*)ptr)) / size;
}

static inline char* strskp(char* dest, const char* src) {
  return dest + strspn(dest, src);
}

// ------------------------------------------------------------------

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

typedef struct node_s {
  const char* name;
  struct node_s** nodes;
  uint32_t nodes_len;
  uint16_t name_len;
  unsigned hh_cc : 1;
} node;

node
  executables = { .name = "exe" },
  objects = { },
  sources = { .name = "SOURCES" },
  headers = { };

int node_cmp(const void* a, const void* b) {
  return strcmp((*(const node**)a)->name, (*(const node**)b)->name);
}

int node_name_cmp(const void* a, const void* b) {
  return strcmp((*(const node**)a)->name, (const char*)b);
}

node* new_node(const char* name) {
  node* p = calloc(1, sizeof(node));
  if (name) {
    size_t len = strlen(name);
    p->name_len = len;
    ++len;
    p->name = memcpy(malloc(len), name, len);
  }
  return p;
}

uint32_t expand_nodes(node* parent) {
  const uint32_t n = parent->nodes_len;
  if (n == 0) {
    parent->nodes = malloc(sizeof(node*));
  } else {
    const uint32_t cap = bit_ceil_32(n);
    if (n == cap) {
      const size_t cap_bytes = cap * sizeof(node*);
      parent->nodes = realloc(parent->nodes, cap_bytes * 2);
    }
  }
  return parent->nodes_len++; // new top index
}

node* add_node(node* parent, node* child) {
  uint32_t n = expand_nodes(parent);
  return parent->nodes[n] = child;
}

node* add_node_at(node* parent, node* child, uint32_t i) {
  uint32_t n = expand_nodes(parent);
  node** a = parent->nodes;
  node** b = a + n;
  a += i;
  while (a < b) {
    node** c = b;
    *c = *--b;
  }
  return *a = child;
}

void print_nodes(const node* a) {
  printf("%s:", a->name);

  node** b = a->nodes;
  node** const end = b + a->nodes_len;
  for (; b != end; ++b)
    printf(" %s", (*b)->name);
  printf("\n");

  b = a->nodes;
  for (; b != end; ++b)
    print_nodes(*b);
}

void append_target(target* t, target* parent) {
  const size_t n = parent->num_deps;
  if (n == 0) {
    parent->deps = malloc(sizeof(target*));
  } else {
    const size_t cap = bit_ceil_64(n);
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

// ------------------------------------------------------------------

const char* scan_code(char** bufp, bool* main) {
  bool new_line = true;
  for (char* a = *bufp; ; ++a) {
    // a += strcspn(a, "\n \t\r#\"/m"); // makes it slower
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
            if (only_quoted_headers) goto next_line;
            close = '>';
            break;
          case '\n':
            new_line = true;
            goto next;
          default:
            goto next_line;
        }
        for (char* b = ++a; ; ++a) {
          const char c = *a;
          if (c == close) { // return included header
            *bufp = a+1;
            *a = '\0';
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
            if (!(a = strstr(a+2, "*/"))) goto end;
            a += 1; // position a at last character of searched substring
            new_line = false;
            goto next;
        }
      case 'm':
        if (!main || *main || strncmp(a+1, "ain", 3)) break;
        char* m = a; // main
        a = strskp(a+4, " \t\r\n");
        if (*a == '(') {
          const char* const buf = *bufp;
          const char* const end = buf + 2;
          const char* b = m;
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
          // unskip blanks, because \n needs to set new_line = true
          // if not a part of int main()
          a = m + 4;
        }
        break;
      case '\0': goto end;
    }

next:
    continue;

next_line:
    if (!(a = strchr(a, '\n'))) goto end;
    new_line = true;
    goto next;
  }

end:
  return NULL;
}

void dir_loop(
  char* path, size_t len, size_t cap, void (*process_file)(const char*)
) {
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
        dir_loop(path, len2, cap, process_file);
        break;
      case DT_REG:
      case DT_LNK:
        process_file(path);
        break;
    }
  }

  closedir(D);
}

const char* path_ext(const char* path) {
  if (path) {
    const char* a = path + strlen(path);
    while (a != path) {
      switch (*--a) {
        case '.': return a+1;
        case '/': return NULL;
      }
    }
  }
  return NULL;
}

/*
void process_header(const char* path) {
  const char* ext = path_ext(path);
  if (!ext || (
    strcmp(ext, "h") &&
    strcmp(ext, "hh") &&
    strcmp(ext, "hpp") &&
    strcmp(ext, "hxx") &&
    strcmp(ext, "h++") &&
    strcmp(ext, "H")
  )) return;

  add_node(&headers, new_node(path));
}
*/

void process_source(const char* path) {
  const char* ext = path_ext(path);
  if (!ext || (
    strcmp(ext, "c") &&
    strcmp(ext, "cc") &&
    strcmp(ext, "cpp") &&
    strcmp(ext, "cxx") &&
    strcmp(ext, "c++") &&
    strcmp(ext, "C")
  )) return;

  printf("%s\n", path);

  const int fd = open(path, O_RDONLY);
  if (fd < 0) ERR("open('%s')", path)

  struct stat sb;
  if (fstat(fd, &sb) < 0) ERR("fstat('%s')", path)
  if (!S_ISREG(sb.st_mode)) ERR("'%s' is not a regular file", path)
  const size_t file_size = sb.st_size;
  if (file_size == 0) return;

  if (file_cap <= file_size) {
    const size_t file_size_ceil = bit_ceil_64(file_size + 1);
    if (file_size_ceil > (1 << 24)) ERR("'%s' is too large", path)
    free(file_buf);
    file_buf = malloc((file_cap = file_size_ceil));
  }

  if (read(fd, file_buf, file_size) < 0) ERR("read('%s')", path)
  file_buf[file_size] = '\0';

  // --------------------------------------------------------

  // add source
  /* name = malloc(len2+1); */
  /* target* src = add_target(name, obj, &sources); */
  /* memcpy(name, path, len2+1); */

  node* source = add_node(&sources, new_node(path));

  bool main = false;
  for (char* buf = file_buf;;) {
    const char* include = scan_code(&buf, &main);
    if (!include) break;

    printf("  %s\n", include);

    static char header_path[8+(1<<10)] = "include/";
    strcpy(header_path+8, include);

    uint32_t i = lower_bound(
      headers.nodes, headers.nodes_len, sizeof(node*), header_path,
      node_name_cmp
    );

    node* header;
    if (
      ( i == headers.nodes_len ||
        strcmp(headers.nodes[i]->name, header_path) != 0
      ) && // new header
      access(header_path, F_OK) == 0 // header file exists
    ) {
      header = add_node_at(&headers, new_node(header_path), i);
    } else {
      header = headers.nodes[i];
    }
    add_node(source, header);
  }

  /*
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
  */

  close(fd);
}

int main(int argc, char** argv) {
  file_buf = malloc((file_cap = 1 << 12));

  char path[1 << 10];
  /*
  if (argc == 1) {
    strcpy(path, "src");
  } else {
    const size_t len = strlen(argv[1]);
    if (len < sizeof(path)) {
      if (len == 0) {
        fprintf(stderr, "empty starting path\n");
        return 1;
      }
      memcpy(path, argv[1], len+1);
    } else {
      fprintf(stderr, "starting path longer than %lu bytes\n", sizeof(path));
      return 1;
    }
  }
  */

  /*
  strcpy(path, "include");
  dir_loop(path, strlen(path), sizeof(path), process_header);
  qsort(headers.nodes, headers.nodes_len, sizeof(node*), node_cmp);
  */

  strcpy(path, "src");
  dir_loop(path, strlen(path), sizeof(path), process_source);

  /* for (uint32_t i=0; i<headers.nodes_len; ++i) */
  /*   TEST_STR(headers.nodes[i]->name); */

  printf("\n");
  print_nodes(&sources);

  free(file_buf);
}
