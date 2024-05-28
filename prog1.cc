#include "backend.hh"
#include "stub.hh"
#include "print.hh"

using namespace lang;

void print_stub(char const* x, u32 which) {
  println("got ", which, ' ', x);
}

void prog1(Backend& b) {

  auto read = b.ph();
  auto print1 = b.ph();
  auto print2 = b.ph();

  b.mov(rdi, rel32(print1));
  b.jmp(rel8(read));

  // read: rdi continuation
  b.label(read);
  b.mov(rax, rdi);
  b.mov(rdi, u64("help"));
  b.jmp(rax);

  // print1: rdi arg
  b.label(print1);
  b.mov(rsi, 1);
  b.mov(rax, u64(print_stub));
  b.call(rax);
  b.mov(rdi, rel32(print2));
  b.jmp(rel8(read));

  // print2: rdi arg
  b.label(print2);
  b.mov(rsi, 2);
  b.mov(rax, u64(print_stub));
  b.call(rax);
  b.ret();
}
