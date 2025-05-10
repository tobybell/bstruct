#include "backend.hh"
#include "stub.hh"

#include <cstdlib>
#include <unistd.h>
#include <type_traits>
#include <algorithm>

using namespace lang;

void parse();

char buf[4096 * 4096];

using StringList = ArrayList<char>;

enum Builtin { U32 };

struct Struct {
  StringList names;
  List<u32> array_size;
  void field(Span<char> name) {
    names.push(name);
    array_size.push(0);
  }
  void field(Span<char> name, u32 size) {
    names.push(name);
    array_size.push(size + 1);
  }
};


// assumes 10 bytes free
u32 to_string(char* out, u32 n) {
  u32 i = 0;
  do {
    out[i++] = '0' + (n % 10);
    n /= 10;
  } while (n);
  std::reverse(out, out + i);
  return i;
}

char const* print_u32(char const* i) {
  char i_buf[10];
  ::write(1, i_buf, to_string(i_buf, *(u32 const*) i));
  return i + 4;
}

char const* print_field(Struct& s, u32 i, char const* it) {
  auto name = s.names[i];
  ::write(1, name.base, name.size);
  ::write(1, "=", 1);

  u32 arr_sz = s.array_size[i];
  if (arr_sz) {
    u32 array_size = arr_sz - 1;
    ::write(1, "[", 1);
    if (array_size) {
      it = print_u32(it);
      for (u32 j = 1; j < array_size; ++j) {
        ::write(1, " ", 1);
        it = print_u32(it);
      }
    }
    ::write(1, "]", 1);
  } else {
    it = print_u32(it);
  }

  return it;
}

void test_case(Struct& s, Span<char> data) {
  char const* it = data.base;
  it = print_field(s, 0, it);
  for (u32 i = 1; i < len(s.names); ++i) {
    ::write(1, " ", 1);
    it = print_field(s, i, it);
  }
  ::write(1, "\n", 1);
}

/*
Generic ABI
- Pass return address as a continuation:

*/

long my_stub(u64 a, u64 b, u64 c) {
  println("my_stub ", a, ' ', b, ' ', c);
  return 24;
}

#define program(name) void name(Backend&);

program(prog1);
program(prog2);

void try_program(void (*prog)(Backend&)) {
  Stream out;
  lang::Backend b {out};
  prog(b);
  Executable exec (out);
  auto fn = exec.as<void>();
  println("Try running it..."_s);
  fn();
}

void test_cpp_generation();

int main() {
  parse();
  test_cpp_generation();

  // try_program(prog1);
  // try_program(prog2);

  // {
  //   Stream out;
  //   lang::Backend b {out};
  //   b.sub(rsp, 8);
  //   call_stub(b, my_stub, 5, 6, 7);
  //   b.add(rsp, 8);
  //   b.ret();
  //   println("Try running it..."_s);
  //   Executable exec (out.bytes);
  //   auto fn = exec.as<void>();
  //   fn();
  // }

  // {
  //   Stream out;
  //   lang::Backend b {out};
  //   b.sub(rsp, 8);
  //   auto ph1 = b.ph();
  //   auto ph2 = b.ph();
  //   b.mov(rax, 0);
  //   b.jmp(rel8(ph1));
  //   b.label(ph2);
  //   b.add(rax, indir<reg64> {rsi, 0});
  //   b.add(rsi, 4);
  //   b.sub(rdi, 1);
  //   b.label(ph1);
  //   b.cmp(rdi, 0);
  //   b.jne(rel8(ph2));
  //   b.push(rax);
  //   call_stub(b, my_stub, 5, 6, 7);
  //   b.pop(rax);
  //   b.add(rsp, 8);
  //   b.ret();
  //   println("Try running it..."_s);
  //   Executable exec (out.bytes);
  //   auto fn = exec.as<u32, int, int*>();

  //   int nums[] {1,2,3,4,5};

  //   auto ans = fn(1, nums);
  //   println("Result was "_s, ans);
  //   ans = fn(2, nums);
  //   println("Result was "_s, ans);
  //   ans = fn(3, nums);
  //   println("Result was "_s, ans);
  // }

  {
    Struct s;
    s.field("name"_s);
    s.field("age"_s);
    s.field("skill"_s);
    s.field("health"_s);
    u32 nums[4] {1,2,3,4};
    test_case(s, {(char const*) nums, 16});
  }

  {
    Struct s;
    s.field("name"_s);
    s.field("age"_s, 3);
    u32 nums[4] {1,2,3,4};
    test_case(s, {(char const*) nums, 16});
  }

  return 0;

  auto n = ::read(0, buf, sizeof(buf));
  check(n >= 0);
  check(::write(1, buf, u32(n)) == n);
}
