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

#define NODE_LOOP(IT, NODE) for ( \
  node ** IT = (NODE)->nodes, \
       ** const end = IT + (NODE)->size; \
  IT != end; ++IT )

// ------------------------------------------------------------------

const bool only_quoted_headers = true;

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

static inline char* strskp(char* dest, const char* src) {
  return dest + strspn(dest, src);
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

size_t unique(
  void* ptr, size_t count, size_t size,
  int (*comp)(const void*, const void*)
) {
  unsigned char *a = ptr, *end = a + count * size;

  if (a == end) return 0;

  unsigned char *b = a;
  while ((a += size) != end)
    if (comp(a, b) != 0 && (b += size) != a)
      memcpy(b, a, size);

  return ((b + size) - ((unsigned char*)ptr)) / size;
}

const char* path_ext(const char* path, size_t len) {
  const char* a = path + len;
  while (a != path) {
    switch (*--a) {
      case '.': return a+1;
      case '/': return NULL;
    }
  }
  return NULL;
}

const char* name_ext(const char* name, size_t len) {
  const char* a = name + len;
  while (a != name)
    if (*--a == '.')
      return a+1;
  return NULL;
}

// ------------------------------------------------------------------

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
  uint32_t size;
  unsigned new : 1;
} node;

node
  executables = { .name = "exe" },
  objects = { },
  sources = { .name = "SOURCES" },
  headers = { .name = "HEADERS" };

node* new_node(const char* name) {
  node* p = calloc(1, sizeof(node));
  if (name) {
    size_t len = strlen(name);
    ++len;
    p->name = memcpy(malloc(len), name, len);
  }
  return p;
}

int ptr_cmp(const void* a, const void* b) {
  return (*(const unsigned char**)a) - (*(const unsigned char**)b);
}

int node_cmp(const void* a, const void* b) {
  return strcmp((*(const node**)a)->name, (*(const node**)b)->name);
}

int node_name_cmp(const void* a, const void* b) {
  return strcmp((*(const node**)a)->name, (const char*)b);
}

int node_name_cmp_no_ext(const void* a, const void* b) {
  const char* path = (*(const node**)a)->name;
  size_t len = strlen(path);
  const char* ext = path_ext(path, len);
  if (ext)
    len = (ext-1) - path;
  return strncmp(path, (const char*)b, len);
}

uint32_t expand_nodes(node* parent) {
  const uint32_t n = parent->size;
  if (n == 0) {
    parent->nodes = malloc(sizeof(node*));
  } else {
    const uint32_t cap = bit_ceil_32(n);
    if (n == cap) {
      const size_t cap_bytes = cap * sizeof(node*);
      parent->nodes = realloc(parent->nodes, cap_bytes * 2);
    }
  }
  return parent->size++; // new top index
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

void print_tree(const node* a, int n) {
  for (int i = 0; i < n; ++i)
    printf("  ");
  printf("%s\n", a->name);

  NODE_LOOP(b, a)
    print_tree(*b, n+1);
}

void print_nodes(const node* a) {
  printf("%s:", a->name);

  node** b = a->nodes;
  node** const end = b + a->size;
  for (; b != end; ++b)
    printf(" %s", (*b)->name);
  printf("\n");

  b = a->nodes;
  for (; b != end; ++b)
    print_nodes(*b);
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

void read_file(const char* path) {
  // file is read into global storage
  // processing of one file must be completed before the next one is read
  // this needs to be considered for recursive functions

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

  close(fd);
}

void process_source_recursive(const char* path, node* source, bool* main) {
  printf("%s\n", path);

  read_file(path);

  // find includes (and main) in the current source
  for (char* buf = file_buf;;) {
    const char* include = scan_code(&buf, main);
    if (!include) break;

    printf("  %s\n", include);

    static char header_path[8+(1<<10)] = "include/";
    strcpy(header_path+8, include);

    uint32_t i = lower_bound(
      headers.nodes, headers.size, sizeof(node*), header_path,
      node_name_cmp
    );

    node* header;
    if (
      ( i == headers.size ||
        strcmp(headers.nodes[i]->name, header_path) != 0
      ) && // new header
      access(header_path, F_OK) == 0 // header file exists
    ) {
      // TODO: implement lower complexity algorithm for headers (set)
      header = add_node_at(&headers, new_node(header_path), i);
      header->new = 1;
    } else {
      header = headers.nodes[i];
    }
    add_node(source, header);
  }

  // process new headers
  NODE_LOOP(it, source) {
    node* header = *it;
    if (header->new) {
      header->new = 0;
      process_source_recursive(header->name, header, NULL);
    }
  }
}

uint32_t tree_count(node* root) {
  uint32_t n = root->size;
  NODE_LOOP(it, root) {
    n += tree_count(*it);
  }
  return n;
}

void add_children(node* root, node* child) {
  NODE_LOOP(it, child) {
    node* grandchild = *it;
    root->nodes[root->size++] = grandchild;
    add_children(root, grandchild);
  }
}

void process_source(const char* path) {
  node* source = add_node(&sources, new_node(path));

  bool main = false;
  process_source_recursive(path, source, &main);

  const uint32_t num_headers = tree_count(source);
  // allocate enough memory to store all indirectly included headers
  if (num_headers > bit_ceil_32(source->size)) {
    const size_t cap_bytes = num_headers * sizeof(node*);
    source->nodes = realloc(source->nodes, cap_bytes);
  }

  NODE_LOOP(it, source) {
    add_children(source, *it);
  }

  qsort(source->nodes, num_headers, sizeof(node*), node_cmp);

  // it is sufficient to compare pointers
  // because there are no redundant header nodes
  source->size =
    unique(source->nodes, num_headers, sizeof(node*), ptr_cmp);

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
        const char* ext = name_ext(name2, name2_len);
        if (ext && !(
          strcmp(ext, "c") &&
          strcmp(ext, "cc") &&
          strcmp(ext, "cpp") &&
          strcmp(ext, "cxx") &&
          strcmp(ext, "c++") &&
          strcmp(ext, "C")
        )) process_source(path);
        break;
    }
  }

  closedir(D);
}

int main(int argc, char** argv) {
  file_buf = malloc((file_cap = 1 << 12));

  char path[1 << 10];
  strcpy(path, "src");
  dir_loop(path, strlen(path), sizeof(path));

  // sort sources for searching
  qsort(sources.nodes, sources.size, sizeof(node*), node_cmp);

  // printf("\n");
  // print_tree(&headers, 0);
  // TEST("%u", tree_count(&headers))

  /* printf("\n"); */
  /* print_tree(&sources, 0); */
  /* TEST("%u", tree_count(&sources)) */
  printf("\n%s\n", sources.name);
  NODE_LOOP(it, &sources) {
    node* source = *it;
    printf("%s\n", source->name);
    NODE_LOOP(it, source) {
      node* header = *it;
      printf("  %s\n", header->name);
    }
  }

  node** const matched_sources = malloc(sizeof(node*) * headers.size);

  // TODO: account for multiple matching files with different extensions

  { node** source_ptr = matched_sources;
    NODE_LOOP(it, &headers) {
      const char* header = (*it)->name + 7; // remove include prefix
      size_t len = strlen(header);
      const char* ext = path_ext(header, len);
      if (ext)
        len = (ext-1) - header;
      memcpy(path + 3, header, len);
      path[len += 3] = '\0';

      uint32_t i = lower_bound(
        sources.nodes, sources.size, sizeof(node*), path,
        node_name_cmp_no_ext
      );

      if (i == sources.size) goto no_match;

      node* source = sources.nodes[i];

      if (strncmp(source->name, path, len) == 0) {
        *source_ptr = source;
      } else {
no_match:
        *source_ptr = NULL;
      }
      ++source_ptr;
    }
  }

  printf("\n");
  { node** source_ptr = matched_sources;
    NODE_LOOP(it, &headers) {
      printf("%s\n", (*it)->name);
      node* source = *source_ptr;
      if (source)
        printf("  %s\n", source->name);
      ++source_ptr;
    }
  }

  free(file_buf);
}
