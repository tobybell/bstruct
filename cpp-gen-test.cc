#include "common.hh"

#include <unistd.h>
#include <sys/wait.h>

namespace {

void wait_to_exit(pid_t p) {
  int status;
  check(waitpid(p, &status, 0) == p);
  check(WIFEXITED(status));
  check(!WEXITSTATUS(status));
}

String read_all(int fd) {
  Stream s;
  static constexpr unsigned chunk = 4096 * 4096;
  while (iptr n_read = read(fd, s.reserve(chunk), chunk)) {
    check(n_read > 0);
    s.size += n_read;
  }
  check(!close(fd));
  return s.take();
}

void write_all(int fd, Str src) {
  check(write(fd, src.base, src.size) == src.size);
  check(!close(fd));
}

struct OneWaySubprocess {
  pid_t p;
  int fd;
};

enum CommunicationDirection: unsigned char { In, Out };

OneWaySubprocess sp_run(char const* const* arg, CommunicationDirection dir) {
  int fds[2];
  check(!pipe(fds));
  pid_t p = fork();
  if (!p) {
    check(dup2(fds[dir], dir) == dir);
    for (unsigned i = 0; i < 2; ++i)
      check(!close(fds[i]));
    char* env[] = {0};
    execve(arg[0], (char**) arg, env);
    abort();
  }
  check(!close(fds[dir]));
  return {p, fds[1 - dir]};
}

template <unsigned N>
OneWaySubprocess sp_run(char const* const (&arg)[N], CommunicationDirection dir) {
  return sp_run((char const* const*) arg, dir);
}

String compile_and_run(Str src) {
  char tmp[] = "/tmp/XXXXXX";
  int fd = mkstemp(tmp);

  auto [cc, in] = sp_run({"/usr/bin/g++", "-o", tmp, "-x", "c++", "-", 0}, In);
  write_all(in, src);
  wait_to_exit(cc);

  auto [exe, out] = sp_run({tmp, 0}, Out);
  String s = read_all(out);
  wait_to_exit(exe);

  printf("exited\n");

  check(!close(fd));
  check(!unlink(tmp));

  return s;
}

}

int main() {
  String output = compile_and_run("#include <stdio.h>\nint main() { printf(\"hi\"); }"_s);
  check(output == "hi"_s);
}
