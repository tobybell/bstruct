#include "common.hh"
#include "parse.hh"

#include <unistd.h>
#include <sys/wait.h>

namespace {

//struct ToCpp {
//  Parser const& p;
//
//  void operator()(Print& s) {
//    for (auto struct_: range(len(p.struct_type))) {
//      auto type = p.struct_type[struct_];
//      auto member_name = p.struct_member_name[struct_];
//      auto member_info = p.struct_member[struct_];
//      sprint(s, "struct "_s, p.types[type], " {\n"_s);
//      for (auto member: range(len(member_info))) {
//        auto name = member_name[member];
//        auto info = member_info[member];
//        if (info.array == NoArray)
//          sprint(s, "  "_s, p.types[info.type], ' ', name, ";\n"_s);
//        else if (info.array == FixedArray)
//          sprint(s, "  "_s, p.types[info.type], ' ', name, '[', info.length, "];\n"_s);
//        else if (info.array == MemberArray) {
//          sprint(s, "  "_s, p.types[info.type], " const* "_s, name, ";\n"_s);
//        } else
//          unreachable;
//      }
//      sprint("};\n\n"_s);
//    }
//
//    auto n_log = len(p.log_type);
//    for (auto log: range(n_log)) {
//      auto type = p.log_type[log];
//      auto member_struct = p.log_member_struct[log];
//      sprint(s, "struct "_s, p.types[type], ": Log<"_s, p.types[p.struct_type[member_struct[0]]]);
//      for (auto member: range(1, len(member_struct)))
//        sprint(s, ", "_s, p.types[p.struct_type[member_struct[member]]]);
//      sprint(s, "> {};\n\n"_s);
//    }
//
//    sprint(s, "int main() {}\n"_s);
//  }
//};

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

  auto [cc, in] = run_subprocess({
      "/usr/bin/g++", "-o", tmp, "-x", "c++", "-", 0}, In);
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
  Library lib = parse(R"(
struct Person
  name u32
  age u32
)");
  println(to_bstruct(lib));

  //Parser p;
  //Print ps;
  //sprint(ps, ToCpp(p));
  //String output = compile_and_run(ps.chars);
  //println("output: "_s, output);
  String output = compile_and_run("#include <stdio.h>\nint main() { printf(\"hi\"); }"_s);
  check(output == "hi"_s);
}
