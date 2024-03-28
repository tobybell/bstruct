#pragma once

#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <utility>

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
    data = reinterpret_cast<char*>(realloc(data, capacity));
  }
  void reserve(u32 amount) {
    grow(size + amount);
  }
  Str str() const { return {data, size}; }
};

inline void print(char c, Stream& s) {
  s.reserve(1);
  s.data[s.size++] = c;
}

void print(u32, Stream&);
void print(u64, Stream&);
void print(char const*, Stream&);

inline void print(Span<char> x, Stream& s) {
  s.reserve(x.size);
  memcpy(&s.data[s.size], x.base, x.size);
  s.size += x.size;
}

template <class... T>
void sprint(Stream& s, T&&... args) {
  (print(std::forward<T>(args), s), ...);
}

template <class... T>
void println(T&&... args) {
  Stream s;
  sprint(s, std::forward<T>(args)..., '\n');
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

template <u32... Values>
struct Indices {};

template <u32 First, u32... Values>
struct IndicesBuilder:
  IndicesBuilder<First - 1, First, Values...> {};

template <u32... Values>
struct IndicesBuilder<0u, Values...> {
  using type = Indices<0u, Values...>;
};

template <u32 Size>
struct MakeIndices {
  using type = typename IndicesBuilder<Size - 1>::type;
};

template <>
struct MakeIndices<0u> {
  using type = Indices<>;
};

template <u32 Size>
using make_indices = typename MakeIndices<Size>::type;

template <class T>
using remove_reference = std::remove_reference_t<T>;

template <class T>
constexpr bool is_lvalue_reference = std::is_lvalue_reference_v<T>;

template <class T>
remove_reference<T>&& move(T&& x) {
  return static_cast<remove_reference<T>&&>(x);
}

template <class T>
T&& forward(remove_reference<T>& x) {
  return static_cast<T&&>(x);
}

template <class T>
T&& forward(remove_reference<T>&& x) {
  static_assert(!is_lvalue_reference<T>);
  return static_cast<T&&>(x);
}

template <class Dst, class Src>
Dst exchange(Dst& dst, Src&& new_value) {
  Dst old_value = move(dst);
  dst = forward<Src>(new_value);
  return old_value;
}

}
