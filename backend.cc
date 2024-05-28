#include "backend.hh"

#include "bytes.hh"
#include "print.hh"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <sys/mman.h>

using std::vector;
using std::uint8_t;

using std::cerr;
using std::endl;
using std::ostream;

namespace lang {

void todo(Str s) {
  println("TODO: "_s, s);
  abort();
}

struct nu8 {
  u8 value;
  constexpr nu8(u8 x): value(x) {}
  constexpr nu8 operator<<(int x) const { return static_cast<u8>(value << x); }
  constexpr nu8 operator|(nu8 x) const { return static_cast<u8>(value | x.value); }
  constexpr nu8 operator&(nu8 x) const { return static_cast<u8>(value & x.value); }
  constexpr nu8 operator*(bool x) const { return static_cast<u8>(value * x); }
  constexpr nu8 operator|(bool x) const { return static_cast<u8>(value | x); }
  constexpr nu8 operator+(bool x) const { return static_cast<u8>(value + x); }
  constexpr operator u8() const { return value; }
};

constexpr nu8 operator"" _uc(u64 x) { return static_cast<u8>(x); }

const char* reg64_names[16] = {
  "rax",
  "rcx",
  "rdx",
  "rbx",
  "rsp",
  "rbp",
  "rsi",
  "rdi",
  "r8",
  "r9",
  "r10",
  "r11",
  "r12",
  "r13",
  "r14",
  "r15",
};

inline bool is_8bit(int32_t x) {
  return x < 128 && x >= -128;
}

char hex(uint8_t b) {
  b &= 0xf;
  if (b <= 9)
    return static_cast<char>('0' + b);
  return static_cast<char>('a' + (b - 10));
}

constexpr auto code(const reg64& r) -> nu8 { return r.id & 7; }
constexpr auto code(const lreg8& r) -> nu8 { return r.id; }
constexpr auto code(const reg16& r) -> nu8 { return static_cast<u8>(r); }
constexpr auto code(const reg32& r) -> nu8 { return static_cast<u8>(r); }
constexpr auto code(const reg8& r) -> nu8 { return static_cast<u8>(r); }
constexpr auto code(const dreg8& r) -> nu8 { return static_cast<u8>(r); }

constexpr auto as_i8(i32 x) -> i8 {
  check(x < 128 && x >= -128);
  return static_cast<i8>(x);
}
constexpr auto as_i8(i64 x) -> i8 {
  check(x < 128 && x >= -128);
  return static_cast<i8>(x);
}
constexpr auto as_i8(u64 x) -> i8 {
  return as_i8(static_cast<i64>(x));
}
constexpr auto as_i32(i64 x) -> i32 {
  check(x >= -2147483648 && x < 2147483648);
  return static_cast<i32>(x);
}
constexpr auto as_i32(u64 x) -> i32 {
  return as_i32(static_cast<i64>(x));
}

void put_one(Stream& s, u64 ofs, u8 byte) {
  s.bytes[ofs] = static_cast<char>(byte);
}

// Create an 8-bit relative reference to placholder `ph` at offset `ofs` in
// backend `b`.
static void rel8(Backend& b, placeholder ph, offset ofs) {
  auto it = b.labels.find(ph);
  if (it != b.labels.end()) {
    auto rel = static_cast<i64>(it->second - (ofs + 1));
    check(rel < 128 && rel > -128);
    put_one(b.output, ofs, static_cast<u8>(rel));
  } else {
    b.refs8[ph].emplace_back(ofs);
  }
}

u8* data_ptr(Stream& s, u64 ofs) {
  return reinterpret_cast<u8*>(&s.bytes[ofs]);
}

u8 const* data_ptr(Stream const& s, u64 ofs) {
  return (u8 const*) (&s.bytes[ofs]);
}

// Create a 32-bit relative reference to placholder `ph` at offset `ofs` in
// backend `b`.
static void rel32(Backend& b, placeholder ph, offset ofs) {
  auto it = b.labels.find(ph);
  if (it != b.labels.end()) {
    auto rel = as_i32(it->second - (ofs + 4));
    encode(rel, data_ptr(b.output, ofs));
  } else {
    b.refs32[ph].emplace_back(ofs);
  }
}

// Define a label for placeholder `p` at offset `x`.
static void label(Backend& b, placeholder p, offset x) {
  b.labels.emplace(p, x);
  auto it8 = b.refs8.find(p);
  if (it8 != b.refs8.end()) {
    for (auto loc: it8->second) {
      auto ofs = as_i8(x - (loc + 1));
      put_one(b.output, loc, ofs & 0xff);
    }
    b.refs8.erase(it8);
  }
  auto it32 = b.refs32.find(p);
  if (it32 != b.refs32.end()) {
    for (auto loc: it32->second) {
      auto ofs = as_i32(x - (loc + 4));
      encode(ofs, data_ptr(b.output, loc));
    }
    b.refs32.erase(it32);
  }
}

void Backend::label(placeholder x) {
  lang::label(*this, x, len(output));
}

static auto g_prefix(reg64 r1, reg64 r2) -> u8 {
  return 0x48_uc | 0x04_uc * (r1.id >= 8) | (r2.id >= 8);
}

void write(Stream& v, auto... x) {
  v.reserve((... + encode_upper_bound(x)));
  auto it = data_ptr(v, len(v));
  ((it = encode(x, it)),...);
  v.grow(reinterpret_cast<char*>(it));
}

void Backend::sub(reg64 r1, reg64 r2) {
  write(output, g_prefix(r2, r1), 0x29_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

void Backend::sub(reg64 r, uint8_t a) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0x83_uc, 0xe8_uc | code(r), a);
}

void Backend::sub(reg16 r, uint8_t a) {
  write(output, 0x66_uc, 0x83_uc, 0xe8_uc | code(r), a);
}

void Backend::neg(reg64 r) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0xf7_uc, 0xd8_uc | code(r));
}

void Backend::shl(reg64 r, uint8_t a) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0xc1_uc, 0xe0_uc | code(r), a);
}

void Backend::shl(reg16 r, uint8_t a) {
  write(output, 0xc1_uc, 0xe0_uc | code(r), a);
}

void Backend::shr(reg16 r, uint8_t a) {
  if (a == 1) {
    write(output, 0x66_uc, 0xd1_uc, 0xe8_uc | code(r));
  } else {
    write(output, 0x66_uc, 0xc1_uc, 0xe8_uc | code(r), a);
  }
}

void Backend::sar(reg64 r, uint8_t a) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0xc1_uc, 0xf8_uc | code(r), a);
}

void Backend::cqo() {
  write(output, 0x48_uc, 0x99_uc);
}

void Backend::push(reg64 r) {
  check(r.id < 8);
  write(output, 0x50_uc | code(r));
}

void Backend::push(reg16 r) {
  write(output, 0x66_uc, 0x50_uc | code(r));
}

void Backend::push(uint8_t n) {
  write(output, 0x6A_uc, n);
}

void Backend::push(uint32_t n) {
  write(output, 0x68_uc, n);
}

void Backend::div(reg64 r) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0xf7_uc, 0xf0_uc | code(r));
}

void Backend::idiv(reg64 r) {
  write(output, 0x48_uc | (r.id >= 8), 0xf7_uc, 0xf8_uc | code(r));
}

void Backend::idiv(reg32 r) {
  write(output, 0xf7_uc, 0xf8_uc | code(r));
}

void Backend::mul(reg64 r) {
  write(output, 0x48_uc | (r.id >= 8), 0xf7_uc, 0xe0_uc | code(r));
}

void Backend::imul(reg64 r) {
  write(output, 0x48_uc | (r.id >= 8), 0xf7_uc, 0xe8_uc | code(r));
}

void Backend::cmp(reg64 r1, reg64 r2) {
  write(output, g_prefix(r2, r1), 0x39_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

void Backend::cmp(reg64 r, uint8_t n) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0x83_uc, 0xf8_uc | code(r), n);
}

void Backend::cmp(reg32 r, uint8_t n) {
  write(output, 0x83_uc, 0xf8_uc | code(r), n);
}

void Backend::cmp(reg8 r, uint8_t n) {
  write(output, 0x80_uc, 0xf8_uc | code(r), n);
}

void Backend::sete(reg8 r) {
  write(output, 0x0f_uc, 0x94_uc, 0xc0_uc | code(r));
}

void Backend::sete(lreg8 r) {
  if (r.id >= 4)
    write(output, 0x40_uc);
  write(output, 0x0f_uc, 0x94_uc, 0xc0_uc | code(r));
}

void Backend::sete(dreg8 r) {
  write(output, 0x40_uc, 0x0f_uc, 0x94_uc, 0xc0_uc | code(r));
}

void Backend::setne(reg8 r) {
  write(output, 0x0f_uc, 0x95_uc, 0xc0_uc | code(r));
}

void Backend::setne(lreg8 r) {
  if (r.id >= 4)
    write(output, 0x40_uc);
  write(output, 0x0f_uc, 0x95_uc, 0xc0_uc | code(r));
}

void Backend::setne(dreg8 r) {
  write(output, 0x40_uc, 0x0f_uc, 0x95_uc, 0xc0_uc | code(r));
}

void Backend::add(reg64 r, int32_t n) {
  write(output, 0x48_uc | (r.id >= 8));
  if (is_8bit(n)) {
    write(output, 0x83_uc, 0xc0_uc | code(r), static_cast<u8>(n));
  } else {
    if (r == rax) {
      write(output, 0x05_uc, n);
    } else {
      write(output, 0x81_uc, 0xc0_uc | code(r), n);
    }
  }
}

void Backend::add(reg16 r, uint16_t n) {
  if (r == ax)
    todo("add special case for add(ax, ...)"_s);
  write(output, 0x66_uc, 0x81_uc, 0xc0_uc | code(r), n);
}

void Backend::add(reg64 r1, reg64 r2) {
  write(output, g_prefix(r2, r1), 0x01_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

struct IndirBundle {
  indir<reg64> r;
  nu8 regs;
};
static u32 encode_upper_bound(IndirBundle const&) { return 6; }
static auto encode(IndirBundle const& x, u8* i) {
  if (!is_8bit(x.r.ofs)) {
    i = write(i, x.regs | 0x80_uc);
    i = code(x.r.r) == 4 ? write(i, 0x24_uc) : i;
    return write(i, x.r.ofs);
  } else if (x.r.ofs != 0 || code(x.r.r) == 5) {
    i = write(i, x.regs | 0x40_uc);
    i = code(x.r.r) == 4 ? write(i, 0x24_uc) : i;
    return write(i, static_cast<u8>(x.r.ofs));
  } else {
    i = write(i, x.regs);
    return code(x.r.r) == 4 ? write(i, 0x24_uc) : i;
  }
}

void Backend::add(reg64 r1, indir<reg64> r2) {
  write(output, g_prefix(r1, r2.r), 0x03_uc, IndirBundle {r2, code(r1) << 3 | code(r2.r)});
}

void Backend::test(reg64 r1, reg64 r2) {
  write(output, g_prefix(r2, r1), 0x85_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

void Backend::test(reg32 r1, reg32 r2) {
  write(output, 0x85_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

void Backend::test(reg16 r1, reg16 r2) {
  write(output, 0x66_uc, 0x85_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

void Backend::jmp(rel8_linkable_address a) {
  write(output, 0xeb_uc, u8(0));
  rel8(*this, a.ph, len(output) - 1);
}

void Backend::jmp(rel32_linkable_address a) {
  write(output, 0xe9_uc, u32(0));
  rel32(*this, a.ph, len(output) - 4);
}

void Backend::jmp(reg64 r) {
  if (r.id >= 8)
    write(output, 0x41_uc);
  write(output, 0xff_uc, 0xe0_uc | code(r));
}

void Backend::jge(rel8_linkable_address a) {
  write(output, 0x7d_uc, u8(0));
  rel8(*this, a.ph, len(output) - 1);
}

void Backend::je(rel8_linkable_address a) {
  write(output, 0x74_uc, u8(0));
  rel8(*this, a.ph, len(output) - 1);
}

void Backend::je(rel32_linkable_address a) {
  write(output, 0x0f_uc, 0x84_uc, u32(0));
  rel32(*this, a.ph, len(output) - 4);
}

void Backend::jne(rel8_linkable_address a) {
  write(output, 0x75_uc, u8(0));
  rel8(*this, a.ph, len(output) - 1);
}

void Backend::jne(rel32_linkable_address a) {
  write(output, 0x0f_uc, 0x85_uc, u32(0));
  rel32(*this, a.ph, len(output) - 4);
}

void Backend::pop(reg64 r) {
  if (r.id >= 8) write(output, 0x41_uc);
  write(output, 0x58_uc | code(r));
}

void Backend::mov(reg64 r, uint32_t n) {
  write(output, 0x48_uc | (r.id >= 8), 0xc7_uc, 0xc0_uc | code(r), n);
}

void Backend::mov(reg64 r, int32_t n) { mov(r, static_cast<uint32_t>(n)); }

void Backend::mov(reg64 r, uint64_t n) {
  write(output, 0x48_uc | (r.id >= 8), 0xb8_uc | code(r), n);
}

void Backend::mov(reg64 r, int64_t n) { mov(r, static_cast<uint64_t>(n)); }

void write_from(Stream& v, u8 const* data, u32 size) {
  v.reserve(size);
  memcpy(&v.bytes[len(v)], data, size);
  v.grow(size);
}

void write_from(Stream& v, Stream const& v2) {
  write_from(v, data_ptr(v2, 0), len(v2));
}

static auto xchg_rax(Backend& b, reg64 r) {
  write(b.output, 0x48_uc | (r.id >= 8), 0x90_uc | code(r));
}

// static auto noop(Backend& b) {
//   write(b.output, 0x90_uc);
// }

void Backend::xchg(reg64 r1, reg64 r2) {
  if (r1 == r2)
    return;  // noop
  if (r1 == rax)
    return xchg_rax(*this, r2);
  if (r2 == rax)
    return xchg_rax(*this, r1);
  write(output, g_prefix(r1, r2), 0x87_uc, 0xc0_uc | (code(r1) << 3) | code(r2));
}

void Backend::mov(reg64 r1, reg64 r2) {
  auto prefix = g_prefix(r2, r1);
  write(output, prefix, 0x89_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

void Backend::mov(reg8 r1, indir<reg64> r2) {
  write(output, 0x8a_uc, IndirBundle {r2, code(r1) << 3 | code(r2.r)});
}

void Backend::mov(dreg8 r1, indir<reg64> r2) {
  write(output, 0x40_uc, 0x8a_uc, IndirBundle {r2, code(r1) << 3 | code(r2.r)});
}

void Backend::mov(reg64 r1, indir<reg64> r2) {
  write(output, g_prefix(r1, r2.r), 0x8b_uc, IndirBundle {r2, code(r1) << 3 | code(r2.r)});
}

void Backend::mov(indir<reg64> r, uint32_t n) {
  if (r.r.id >= 8)
    write(output, 0x41_uc);
  write(output, 0xc7_uc, IndirBundle {r, code(r.r)}, n);
}

void Backend::mov(indir<reg64> r1, reg64 r2) {
  write(output, g_prefix(r2, r1.r), 0x89_uc, IndirBundle {r1, code(r2) << 3 | code(r1.r)});
}

void Backend::mov(indir<reg64> r, uint8_t n) {
  if (r.r.id >= 8)
    write(output, 0x41_uc);
  write(output, 0xc6_uc, IndirBundle {r, code(r.r)}, n);
}

void Backend::mov(indir<reg64> r1, reg8 r2) {
  if (r1.r.id >= 8)
    write(output, 0x41_uc);
  write(output, 0x88_uc, IndirBundle {r1, code(r2) << 3 | code(r1.r)});
}

auto g_prefix(reg64 r1, dreg8 r2) -> uint8_t {
  return r1.id >= 8 ? 0x41_uc : 0x40_uc;
}

void Backend::mov(indir<reg64> r1, dreg8 r2) {
  write(output, g_prefix(r1.r, r2), 0x88_uc, IndirBundle {r1, code(r2) << 3 | code(r1.r)});
}

void Backend::mov(reg32 r1, indir<reg64> r2) {
  check(is_8bit(r2.ofs));  // why is this here?
  check(r2.r.id < 8);  // why is this here?
  write(output, 0x8b_uc, IndirBundle {r2, code(r1) << 3 | code(r2.r)});
}

void Backend::mov(reg64 r, rel32_linkable_address a) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0x8d_uc, 0x00_uc | (code(r) << 3) | 0b101_uc, u32(0));
  rel32(*this, a.ph, len(output) - 4);
}

// TODO: Turn this into explicit lea from rip.
void Backend::lea(reg64 r, int32_t ofs) {
  check(r.id < 8);  // why is this here?
  write(output, 0x48_uc | (r.id >= 8), 0x8d_uc, 0x00_uc | (code(r) << 3) | 0b101_uc, ofs);
}

void Backend::lea(reg64 r1, indir<reg64> r2) {
  check(r1.id < 16 && r2.r.id < 16); // why is this here?
  write(output, g_prefix(r1, r2.r), 0x8d_uc, IndirBundle {r2, code(r1) << 3 | code(r2.r)});
}

void Backend::syscall() {
  write(output, 0x0f_uc, 0x05_uc);
}

void Backend::call(rel32_linkable_address a) {
  write(output, 0xe8_uc, u32(0));
  rel32(*this, a.ph, len(output) - 4);
}

void Backend::call(reg64 r) {
  if (r.id >= 8)
    write(output, 0x41_uc);
  write(output, 0xff_uc, 0xd0_uc | code(r));
}

void Backend::xor_(reg64 r, uint8_t n) {
  check(r.id < 8);
  write(output, 0x48_uc | (r.id >= 8), 0x83_uc, 0xf0_uc | code(r), n);
}

void Backend::xor_(reg64 r1, reg64 r2) {
  check(r1.id < 8 && r2.id < 8);
  write(output, 0x48_uc, 0x31_uc, 0xc0_uc | (code(r2) << 3) | code(r1));
}

void Backend::ret() {
  write(output, 0xc3_uc);
}

void Backend::literal(Str s) {
  write_from(output, reinterpret_cast<u8 const*>(s.base), s.size);
}

void Backend::dump_output() {
  size_t i = 0;
  while (i < len(output)) {
    for (int c = 0; c < 16; ++c) {
      uint8_t b = u8(output.bytes[i]);
      cerr << hex(b >> 4) << hex(b) << ' ';
      i += 1;
      if (i == len(output)) break;
    }
    cerr << endl;
  }
}

placeholder Backend::ph() { return {placeholder_counter++}; }

void append(Backend& b1, const Backend& b2) {
  auto& b1o = b1.output;
  auto& b2o = b2.output;
  auto b1n = len(b1o);

  write_from(b1o, b2o);

  // Inherit all references from b2.
  for (auto& [ph, locs]: b2.refs8)
    for (auto loc: locs)
      rel8(b1, ph, b1n + loc);
  for (auto& [ph, locs]: b2.refs32)
    for (auto loc: locs)
      rel32(b1, ph, b1n + loc);

  // Inherit all labels from b2.
  for (auto [ph, ofs]: b2.labels)
    label(b1, ph, b1n + ofs);
}

Executable::Executable(Str output) {
  data = mmap(0, len(output), PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  size = len(output);
  memcpy(data, output.base, len(output));
}

Executable::~Executable() {
  munmap(data, size);
}

}
