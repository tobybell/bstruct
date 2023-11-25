#pragma once

#include "common.hh"

namespace lang {

struct Bytes {
  u8* data;
  u64 capacity;
  u64 size;
  Bytes(): data((u8*) malloc(8)), capacity(8), size(0) {}
  ~Bytes() { free(data); }
  Bytes(Bytes const&) = delete;
  constexpr Bytes(Bytes&& x):
      data(x.data), capacity(x.capacity), size(x.size) {
    x.data = nullptr;
  }
  auto operator=(Bytes&& x) -> Bytes& {
    data = x.data;
    capacity = x.capacity;
    size = x.size;
    x.data = nullptr;
    return *this;
  }
  constexpr u8 const& operator[](u64 index) const { return data[index]; }
  constexpr u8& operator[](u64 index) { return data[index]; }
};

auto grow(Bytes& v, u64 new_size) -> void;

auto push_from(Bytes&, u8 const*, u64 size) -> void;

inline auto len(Bytes const& x) { return x.size; }

inline auto push_from(Bytes& b, Bytes const& other) -> void {
  return push_from(b, other.data, other.size);
}

inline auto push(Bytes& v, auto... x) -> void {
  u8 m[] {x...};
  push_from(v, m, sizeof...(x));
}

struct Bytes64 {
  Bytes bytes;
  auto data() const -> u64 const* { return reinterpret_cast<u64*>(bytes.data); }
  auto data() -> u64* { return reinterpret_cast<u64*>(bytes.data); }
  constexpr auto& operator[](u64 index) const { return data()[index]; }
  constexpr auto& operator[](u64 index) { return data()[index]; }
};

inline auto len(Bytes64 const& x) { return x.bytes.size / sizeof(u64); }

inline auto push_from(Bytes64& v, u64 const* m, u64 size) -> void {
  push_from(v.bytes, reinterpret_cast<u8 const*>(m), size * sizeof(u64));
}

inline auto push_from(Bytes64& b, Bytes64 const& other) -> void {
  return push_from(b.bytes, other.bytes);
}

inline auto push(Bytes64& v, auto... x) -> void {
  u64 m[] {x...};
  push_from(v, m, sizeof...(x));
}

// Encoding stuff

constexpr auto as_u(i64 x) -> u64 { return static_cast<u64>(x); }
constexpr auto as_u(i32 x) -> u32 { return static_cast<u32>(x); }
constexpr auto as_u(i8 x) -> u8 { return static_cast<u8>(x); }

constexpr auto encode_upper_bound(u64 const&) -> u64 { return 8; }
constexpr auto encode_upper_bound(i64 const&) -> u64 { return 8; }
constexpr auto encode_upper_bound(u32 const&) -> u64 { return 4; }
constexpr auto encode_upper_bound(i32 const&) -> u64 { return 4; }
constexpr auto encode_upper_bound(u8 const&) -> u64 { return 1; }
constexpr auto encode_upper_bound(i8 const&) -> u64 { return 1; }

constexpr auto encode(u64 x, u8* i) {
  *i++ = static_cast<u8>(x >> 0);
  *i++ = static_cast<u8>(x >> 8);
  *i++ = static_cast<u8>(x >> 16);
  *i++ = static_cast<u8>(x >> 24);
  *i++ = static_cast<u8>(x >> 32);
  *i++ = static_cast<u8>(x >> 40);
  *i++ = static_cast<u8>(x >> 48);
  *i++ = static_cast<u8>(x >> 56);
  return i;
}
constexpr auto encode(i64 x, u8* i) { return encode(as_u(x), i); }
constexpr auto encode(u32 x, u8* i) {
  *i++ = static_cast<u8>(x >> 0);
  *i++ = static_cast<u8>(x >> 8);
  *i++ = static_cast<u8>(x >> 16);
  *i++ = static_cast<u8>(x >> 24);
  return i;
}
constexpr auto encode(i32 x, u8* i) { return encode(as_u(x), i); }
constexpr auto encode(u8 x, u8* i) { *i++ = x; return i; }
constexpr auto encode(i8 x, u8* i) { return encode(as_u(x), i); }

inline auto write(u8* it, auto... x) -> u8* {
  ((it = encode(x, it)),...);
  return it;
}

inline auto write(Bytes& v, auto... x) -> void {
  grow(v, v.size + (... + encode_upper_bound(x)));
  auto it = &v[v.size];
  ((it = encode(x, it)),...);
  v.size = static_cast<u64>(it - v.data);
}

}
