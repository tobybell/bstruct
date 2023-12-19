#include "backend.hh"
#include "stub.hh"

using namespace lang;

namespace {
  
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
  void (*printers[4])(u64 const*) {
      print_integer, print_string, print_list, print_object};
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

thread_local struct FrameContext* ctx {};

struct FrameContext {
  Backend& b;
  u32 track_rsp {};
  FrameContext(Backend& b_): b(b_) {
    check(exchange(ctx, this) == nullptr);
  }
  ~FrameContext() {
    check(exchange(ctx, nullptr) == this);
  }
};

void push_imm64(u64 imm) {
  ctx->b.mov(rax, imm);
  ctx->b.push(rax);
  ctx->track_rsp += 8;
}

struct StackValue {
  u32 rsp_offset;
};

StackValue string(Str str) {
  check(len(str) <= 8);
  u64 bytes;
  memcpy(&bytes, str.base, len(str));
  push_imm64(bytes);
  push_imm64((u64(len(str)) << 8) | 1);
  return {ctx->track_rsp};
}

StackValue integer(u64 value) {
  push_imm64(value << 8);
  return {ctx->track_rsp};
}

void load(reg64 r, StackValue v) {
  ctx->b.lea(r, indir<reg64> {rsp, i32(ctx->track_rsp - v.rsp_offset)});
}

void push(StackValue v) {
  load(rax, v);
  ctx->b.push(rax);
  ctx->track_rsp += 8;
}

StackValue list(std::initializer_list<StackValue> items) {
  for (u32 i = u32(items.size()); i--;)
    push(items.begin()[i]);
  push_imm64((u64(items.size()) << 8) | 2);
  return {ctx->track_rsp};
}

StackValue
object(StackValue keys, std::initializer_list<StackValue> values) {
  for (u32 i = u32(values.size()); i--;)
    push(values.begin()[i]);
  push(keys);
  ctx->b.push(3);
  ctx->track_rsp += 8;
  return {ctx->track_rsp};
}

void print(StackValue v) {
  load(rdi, v);
  ctx->b.mov(rax, u64(print_stub));
  ctx->b.call(rax);
}

}

void prog2(Backend& b) {
  FrameContext ctx {b};

  auto v1 = integer(13423);
  auto v2 = string("hello"_s);

  auto v3 = list({v1, v2});

  print(v3);

  auto name = string("name"_s);
  auto age = string("age"_s);
  auto keys = list({name, age});

  auto toby = string("Toby"_s);
  auto ts = integer(26);
  auto obj = object(keys, {toby, ts});

  print(obj);

  b.add(rsp, i32(ctx.track_rsp));
  b.ret();
}
