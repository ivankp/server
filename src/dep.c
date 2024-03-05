#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>

#define STR1(x) #x
#define STR(x) STR1(x)

#define CAT1(a,b) a##b
#define CAT(a,b) CAT1(a,b)

#define ERROR_PREF __FILE__ ":" STR(__LINE__) ": "

#define GET_MACRO(_1, _2, NAME, ...) NAME

#define ERR(...) GET_MACRO(__VA_ARGS__, ERR2, ERR1)(__VA_ARGS__)
#define ERR1(FCN) ERR2(err,FCN)
#define ERR2(LABEL, FCN) { \
  const int e = errno; \
  fprintf( \
    stderr, \
    ERROR_PREF FCN "(): [%d] %s", \
    e, strerror(e) \
  ); \
  goto LABEL; \
}

#define FATAL(FCN) { \
  const int e = errno; \
  fprintf( \
    stderr, \
    ERROR_PREF FCN "(): [%d] %s", \
    e, strerror(e) \
  ); \
  exit(1); \
}

char* memapp(char* dest, const char* src, size_t count) {
  memcpy(dest, src, count);
  return dest + count;
}

char* srcs[1 << 5];
size_t num_srcs = 0;

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

int src_loop(char* path, size_t len, size_t cap) {
  DIR* D = opendir(path);
  if (!D) ERR("opendir")

  char* path2 = path + len;
  *path2 = '/';
  ++path2;
  ++len;

  const size_t rem = cap - len - 1; // 1 for null

  int pipes[2][2]; // 0 - read end, 1 - write end

  for (struct dirent *d; (d = readdir(D)); ) {
    const char* const name2 = d->d_name;

    if (!name2[strspn(name2, ".")]) // . ..
      continue;

    const size_t name2_len = strlen(name2);
    if (rem < name2_len) {
      fprintf(stderr, "path is too long\n");
      goto err;
    }

    const size_t len2 = len + name2_len;
    memcpy(path2, name2, name2_len+1);

    switch (d->d_type) {
      case DT_DIR:
        const int ret = src_loop(path, len2, cap);
        if (ret) return ret;
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
          memcpy( (srcs[num_srcs++] = malloc(len2+1)), path, len2+1 );

          // TODO: spaces in file names
          // TODO: exec instead of system
          static char cmd[1 << 10];
          sprintf(cmd, "grep -q '^\\s*int\\s\\+main\\s*(' '%s'", path);
          const int main = system(cmd) == 0;
          if (main) {
          //   // TODO: exec instead of system
          //   /* sprintf(cmd, "g++ -std=c++20 -Iinclude -MM '%s' -MT ''", path); */
          //   /* system(cmd); */

          //   sprintf(cmd, "make -n '.build/%.*s.o' -W '%s'",
          //     (int)(len2 - 5 - (name2_len - (ext - name2))), path+4, path
          //   );
          //   system(cmd);

            size_t target_len = len2 + 4 - (name2_len - (ext - name2));
            char* target = malloc(target_len);
            memapp(
              memapp(
                memapp(
                  target, ".build/", 7
                ), path+4, target_len-9
              ), ".o", 2
            );

            char* args[] = {
              "make", "-n", target, "-W", path, NULL
            };

            if (pipe(pipes[0])) ERR("pipe")
            if (pipe(pipes[1])) ERR(err_close_0, "pipe")

            const pid_t pid = fork();
            if (pid < 0) ERR(err_close_1, "fork")
            if (pid == 0) { // this is the child process
              if (dup2(pipes[0][0], STDIN_FILENO ) < 0) FATAL("dup2")
              if (dup2(pipes[1][1], STDOUT_FILENO) < 0) FATAL("dup2")
              // Note: dup2 doesn't close original fd

              // Note: fds are open separately in both processes
              close(pipes[0][0]);
              close(pipes[0][1]);
              close(pipes[1][0]);
              close(pipes[1][1]);

              // TODO: how to signal child process failure???

              execvp(args[0], args); // child process is replaced by exec()

              // The exec() functions only return if an error has occurred.
              fprintf(stderr, "command:");
              for (int j=0; args[j]; ++j)
                fprintf(stderr, " '%s'", args[j]);
              fprintf(stderr, "\n");
              FATAL("execvp")
            }
            // TODO: https://linux.die.net/man/2/waitpid

            // original process
            close(pipes[0][0]); // read end of input pipe
            close(pipes[1][1]); // write end of output pipe

            close(pipes[0][1]); // write end of input pipe

            char buf[1 << 10];
            const int nread = read(pipes[1][0], buf, sizeof(buf));

            printf("%.*s\n", nread, buf);

            close(pipes[1][0]); // read end of the output pipe
          }
        }
        break;
    }
  }

  closedir(D);

  return 0;

err_close_1:
  close(pipes[1][0]);
  close(pipes[1][1]);

err_close_0:
  close(pipes[0][0]);
  close(pipes[0][1]);

err:
  return 1;
}

int main(int argc, char** argv) {
  char path[1 << 10] = "src";

  int ret = src_loop(path, strlen(path), sizeof(path));
  if (ret) return ret;

  for (char** p = srcs; *p; ++p) {
    printf("%s\n", *p);
    free(*p);
  }
}
