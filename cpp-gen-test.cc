#include "common.hh"
#include "parse.hh"

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void wait_to_exit(pid_t p) {
  int status;
  pid_t ans;
  do {
    ans = waitpid(p, &status, 0);
  } while (ans == -1 && errno == EINTR);
  check(ans == p);
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

OneWaySubprocess run_subprocess(
    char const* const* arg, CommunicationDirection dir) {
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
OneWaySubprocess run_subprocess(
    char const* const (&arg)[N], CommunicationDirection dir) {
  return run_subprocess((char const* const*) arg, dir);
}

String compile_and_run(Str src) {
  char tmp[] = "/tmp/XXXXXX";
  int fd = mkstemp(tmp);

  auto [cc, in] = run_subprocess(
      {"/usr/bin/g++", "-o", tmp, "-std=c++17", "-x", "c++", "-", 0}, In);
  write_all(in, src);
  wait_to_exit(cc);

  auto [exe, out] = run_subprocess({tmp, 0}, Out);
  String s = read_all(out);
  wait_to_exit(exe);

  check(!close(fd));
  check(!unlink(tmp));

  return s;
}

}

void test_cpp_generation() {
  {
    Library lib = parse(R"(
struct Person
  name u32
  age u32
)"_s);

    Print p;
    sprint(p, "using u32 = unsigned;\n"_s, to_cpp(lib), R"(
#include <unistd.h>
int main() {
  Person p {23, 45};
  write(1, &p, sizeof(p));
}
)"_s);
    String output = compile_and_run(p.chars);
    check(output == Span((char[]) {23, 0, 0, 0, 45, 0, 0, 0}));
  }
  {
    Library lib = parse(R"(
struct Person
  name[4] u8
)"_s);
    Print p;
    sprint(p, to_cpp(lib), R"(
#include <unistd.h>
int main() {
  Person p {{5,6,7,8}};
  write(1, &p, sizeof(p));
}
)"_s);
    String output = compile_and_run(p.chars);
    check(output == Span((char[]) {5, 6, 7, 8}));
  }
  {
    Library lib = parse(R"(
struct Person
  len u32
  name[len] u8
)"_s);
    Print p;
    sprint(
        p, to_cpp(lib),
        R"(
#include <unistd.h>
int main() {
  u8 v[] {5, 6, 7, 8};
  Person p {4, v};
  u32 n = p.serialized_size();
  auto buf = new char[n];
  if (!(p.serialize(buf) == buf + n))
    abort();
  write(1, buf, n);
}
)"_s);
    String output = compile_and_run(p.chars);
    check(output == Span((char[]) {4, 0, 0, 0, 5, 6, 7, 8}));
  }
  {
    Library lib = parse(R"(
struct A
  one u32
  two u32
)"_s);
    Print p;
    sprint(
        p, to_cpp(lib),
        R"(
#include <unistd.h>
#include <string.h>
int main() {
  A x {3, 4};
  auto n = x.member_names;
  char c = ',';
  for (u32 i = 0; i < x.member_count; ++i) {
    write(1, n, strlen(n));
    write(1, &c, 1);
    n += strlen(n) + 1;
  }
}
)"_s);
    println("TODO: Add value printing to this test");
    String output = compile_and_run(p.chars);
    check(output == "one,two,"_s);
  }
}
