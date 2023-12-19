#include "backend.hh"
#include "stub.hh"

using namespace lang;

thread_local Stream s;

void print_value(u64 const*);

void print_integer(u64 const* words) {
  u64 value = words[0] >> 8;
  print(value, s);
}

void print_string(u64 const* words) {
  u64 length = words[0] >> 8;
  check(length < 4294967296);
  Str chars {(char const*) (words + 1), u32(length)};
  sprint(s, '"', chars, '"');
}

void print_list(u64 const* words) {
  u64 length = words[0] >> 8;
  sprint(s, '[');
  for (u32 i {}; i < length; ++i) {
    if (i)
      sprint(s, ", "_s);
    print_value(reinterpret_cast<u64 const*>(words[1 + i]));
  }
  sprint(s, ']');
}

void print_object(u64 const* words) {
  u64 const* keys = reinterpret_cast<u64 const*>(words[1]);
  u8 keys_type = u8(keys[0]);
  check(keys_type == 2);
  u64 length = keys[0] >> 8;
  sprint(s, '{');
  for (u32 i {}; i < length; ++i) {
    if (i)
      sprint(s, ", "_s);
    print_value(reinterpret_cast<u64 const*>(keys[1 + i]));
    sprint(s, ": "_s);
    print_value(reinterpret_cast<u64 const*>(words[2 + i]));
  }
  sprint(s, '}');
}

void print_value(u64 const* object) {
  // integer (u8 0, u56 value)
  // string (u8 1, u56 length, u8 chars[length])
  // list (u8 2, u56 length, u64 ptrs[length])
  // object (u64 3, u64 keys_ptr, u64 value_ptrs[length])
  void (*printers[4])(u64 const*) {print_integer, print_string, print_list, print_object};
  u8 type = u8(object[0]);
  check(type < 4);
  printers[type](object);
}

void print_stub(u64 const* words) {
  s.size = 0;
  print_value(words);
  sprint(s, '\n');
  write(1, s.data, s.size);
}

thread_local u32 track_rsp {};

void push_imm64(Backend& b, u64 imm) {
  b.mov(rax, imm);
  b.push(rax);
  track_rsp += 8;
}

void string(Backend& b, reg64 r, Str str) {
  check(len(str) <= 8);
  u64 bytes;
  memcpy(&bytes, str.base, len(str));
  push_imm64(b, bytes);
  push_imm64(b, (u64(len(str)) << 8) | 1);
  b.mov(r, rsp);
}

void integer(Backend& b, reg64 r, u64 value) {
  push_imm64(b, value << 8);
  b.mov(r, rsp);
}

void prog2(Backend& b) {
  track_rsp = 0;

  integer(b, rdi, 13423);
  string(b, rsi, "hello"_s);

  // list
  b.push(rdi);
  b.push(rsi);
  track_rsp += 16;
  push_imm64(b, (2ull << 8) | 2);
  b.mov(rdi, rsp);

  b.mov(rax, u64(print_stub));
  b.call(rax);

  // object
  string(b, rsi, "name"_s);
  string(b, rcx, "age"_s);
  b.push(rcx);
  b.push(rsi);
  track_rsp += 16;
  push_imm64(b, (2ull << 8) | 2);
  b.mov(rsi, rsp);

  string(b, rdi, "Toby"_s);
  integer(b, rcx, 26);
  b.push(rcx);
  b.push(rdi);
  b.push(rsi);
  b.push(3);
  track_rsp += 32;
  b.mov(rdi, rsp);

  b.mov(rax, u64(print_stub));
  b.call(rax);

  b.add(rsp, i32(track_rsp));
  b.ret();
}
