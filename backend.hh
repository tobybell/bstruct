#pragma once

#include "bytes.hh"

#include <string>
#include <map>
#include <vector>
#include <ostream>
#include <cassert>

namespace lang {

template <class T>
struct indir {
  T r;
  int32_t ofs;
};

static constexpr size_t n_regs = 16;

struct reg64 {
  uint8_t id;
  constexpr reg64(): id(static_cast<uint8_t>(0xff)) {}
  constexpr explicit reg64(uint8_t id): id(id) {}
  constexpr indir<reg64> operator[](int32_t ofs) const { return {*this, ofs}; }
  constexpr bool operator<(const reg64& b) const { return id < b.id; }
  constexpr bool operator==(const reg64& b) const { return id == b.id; }
  constexpr bool operator!=(const reg64& b) const { return id != b.id; }
};

static constexpr reg64 rax {0};
static constexpr reg64 rcx {1};
static constexpr reg64 rdx {2};
static constexpr reg64 rbx {3};
static constexpr reg64 rsp {4};
static constexpr reg64 rbp {5};
static constexpr reg64 rsi {6};
static constexpr reg64 rdi {7};
static constexpr reg64 r8  {8};
static constexpr reg64 r9  {9};
static constexpr reg64 r10 {10};
static constexpr reg64 r11 {11};
static constexpr reg64 r12 {12};
static constexpr reg64 r13 {13};
static constexpr reg64 r14 {14};
static constexpr reg64 r15 {15};

extern const char* reg64_names[16];

inline std::ostream& operator<<(std::ostream& os, const reg64& r) {
  return os << reg64_names[r.id];
}

enum reg32 {
  eax = 0,
  ecx = 1,
  edx = 2,
  ebx = 3,
  esp = 4,
  ebp = 5,
  esi = 6,
  edi = 7,
};

enum reg16 {
  ax = 0,
  cx = 1,
  dx = 2,
  bx = 3,
};

enum reg8 {
  al = 0,
  cl = 1,
  dl = 2,
  bl = 3,
  ah = 4,
  ch = 5,
  dh = 6,
  bh = 7,
};

struct lreg8 {
  // al = 0,
  // cl = 1,
  // dl = 2,
  // bl = 3,
  // spl = 4,
  // bpl = 5,
  // sil = 6,
  // dil = 7,
  uint8_t id;
  lreg8(uint8_t id): id(id) {}
};

enum dreg8 {
  spl = 4,
  bpl = 5,
  sil = 6,
  dil = 7,
};

enum fd_t {
  stderr_ = 0,
  stdout_ = 1
};

enum class syscall_t {
  exit  = 0x02000001,
  write = 0x02000004,
  mmap  = 0x020000c5
};

template <class T>
struct unique_int {
  u32 val;
  constexpr unique_int(u32 val): val(val) {}
  constexpr bool operator==(const unique_int& y) const { return val == y.val; }
  constexpr bool operator!=(const unique_int& y) const { return val != y.val; }
  constexpr bool operator<(const unique_int& y) const { return val < y.val; }
};

/** Represents a unique place in the output. */
struct placeholder: unique_int<placeholder> {
  using unique_int::unique_int;
};

/** An offset into a backend block. */
using offset = u64;

struct rel8_linkable_address {
  placeholder ph;
};

struct rel32_linkable_address {
  placeholder ph;
};

constexpr rel8_linkable_address rel8(placeholder x) { return {x}; }

constexpr rel32_linkable_address rel32(placeholder x) { return {x}; }

inline lreg8 lowest8(reg64 r) {
  assert(r.id < 8); // TODO: Need to support for extended ones?
  return lreg8(r.id);
}

struct Backend {

  Bytes output;

  u32 placeholder_counter;

  // Create a new unique placeholder.
  placeholder ph();

  // Labels that have been placed in this backend block.
  std::map<placeholder, offset> labels;

  // Locations in this backend block that refer to specific placeholders.
  std::map<placeholder, std::vector<offset>> refs8;
  std::map<placeholder, std::vector<offset>> refs32;

  void label(placeholder x);

  void setup();

  void literal(const std::string& s);

  void cqo();

  void neg(reg64 r);

  void push_(reg64 r);
  void push_(reg16 r);
  void push_(uint8_t n);
  void push_(uint32_t n);
  void push_(int32_t n) { return push_(uint32_t(n)); }

  void div(reg64 r);
  void idiv(reg64 r);
  void idiv(reg32 r);
  void mul(reg64 r);
  void imul(reg64 r);

  void jmp(rel8_linkable_address);
  void jmp(rel32_linkable_address);
  void je(rel8_linkable_address);
  void je(rel32_linkable_address);
  void jne(rel8_linkable_address);
  void jne(rel32_linkable_address);
  void jge(rel8_linkable_address);

  void shl(reg64 r, uint8_t a);
  void shl(reg16 r, uint8_t a);
  void shr(reg16 r, uint8_t a);
  void sar(reg64 r, uint8_t a);

  void add(reg64 r, int32_t n);
  void add(reg16 r, uint16_t n);
  void add(reg64 r1, reg64 r2);
  void add(reg64 r1, indir<reg64> r2);

  void sub(reg64 r1, reg64 r2);
  void sub(reg64 r, uint8_t a);
  void sub(reg16 r, uint8_t a);

  void test(reg16 r1, reg16 r2);
  void test(reg32 r1, reg32 r2);
  void test(reg64 r1, reg64 r2);
  void cmp(reg64 r1, reg64 r2);
  void cmp(reg64 r, uint8_t n);
  void cmp(reg32 r, uint8_t n);
  void cmp(reg8 r, uint8_t n);
  void sete(reg8 r);
  void sete(lreg8 r);
  void sete(dreg8 r);
  void setne(reg8 r);
  void setne(lreg8 r);
  void setne(dreg8 r);

  void pop(reg64 r);

  void xchg(reg64 r1, reg64 r2);

  void mov(reg64 r1, reg64 r2);
  void mov(reg64 r, uint32_t n);
  void mov(reg64 r, int32_t n);
  void mov(reg64 r, uint64_t n);
  void mov(reg64 r, int64_t n);
  void mov(reg8 r1, indir<reg64> r2);
  void mov(dreg8 r1, indir<reg64> r2);
  void mov(indir<reg64> r1, reg64 r2);
  void mov(indir<reg64>, uint32_t);
  void mov(indir<reg64>, uint8_t);
  void mov(indir<reg64>, reg8);
  void mov(indir<reg64>, dreg8);
  void mov(reg64 r1, indir<reg64> r2);
  void mov(reg32 r1, indir<reg64> r2);
  void mov(reg64 r, rel32_linkable_address a);

  // Convenient wrappers to prevent some ambiguity errors.
  void mov(indir<reg64> r, char n) { mov(r, uint8_t(n)); }

  void lea(reg64 r, int32_t ofs);
  void lea(reg64 r1, indir<reg64> r2);

  void syscall();

  void call(rel32_linkable_address a);
  void call(reg64);

  void ret();

  void xor_(reg64 r, uint8_t n);
  void xor_(reg64 r1, reg64 r2);

  void dump_output();
};

void append(Backend& b1, const Backend& b2);

void try_running_it(Bytes& output);

}
