#pragma once

#include <cstdlib>
#include <unistd.h>
#include <cstring>

namespace lang {

using i64 = signed long long;
using i32 = signed int;
using i16 = signed short;
using i8 = signed char;

using u64 = unsigned long long;
using u32 = unsigned int;
using u16 = unsigned short;
using u8 = unsigned char;

static_assert(sizeof(u64) == 8);
static_assert(sizeof(u32) == 4);
static_assert(sizeof(u16) == 2);
static_assert(sizeof(u8) == 1);

using uptr = unsigned long;

static_assert(sizeof(uptr) == sizeof(void*));

constexpr void check(bool condition) {
  if (!condition)
    abort();
}

template <class T>
struct Span {
  T const* base;
  u32 size;
  T const* begin() const { return base; }
  T const* end() const { return base + size; }
  T const& operator[](u32 index) const { return base[index]; }
  friend u32 len(Span const& x) { return x.size; }
  explicit operator bool() const { return size; }
  template <class S>
  Span<S> reinterpret() const {
    static_assert(sizeof(S) == sizeof(T));
    return {reinterpret_cast<S const*>(base), size};
  }
};

using Str = Span<char>;

struct Stream {
  char* data {};
  u32 capacity {};
  u32 size {};
  Stream() = default;
  Stream(Stream const&) = delete;
  ~Stream() {
    free(data);
  }
  void grow(u32 needed) {
    if (needed <= capacity)
      return;
    if (!capacity)
      capacity = needed;
    else
      while (capacity < needed)
        capacity *= 2;
    data = reinterpret_cast<char*>(realloc(data, needed));
  }
  void reserve(u32 amount) {
    grow(size + amount);
  }
  Str str() const { return {data, size}; }
};

inline void print(Span<char> x, Stream& s) {
  s.reserve(x.size);
  memcpy(&s.data[s.size], x.base, x.size);
  s.size += x.size;
}

template <class... T>
void println(T&&... args) {
  Stream s;
  (print(args, s), ...);
  write(1, s.data, s.size);
}

constexpr Span<char> operator""_s(char const* s, uptr n) {
  return {s, u32(n)};
}

template <class T = void>
[[noreturn]]
static T todo(Span<char> m) {
  println("TODO: "_s, m);
  abort();
}

struct Range {
  u32 size;
  Range begin() const { return {0}; }
  Range end() const { return {size}; }
  u32 operator*() const { return size; }
  void operator++() { ++size; }
};

constexpr Range range(u32 stop) { return {stop}; }

}
