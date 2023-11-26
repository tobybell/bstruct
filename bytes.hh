#pragma once

#include "common.hh"

namespace lang {

constexpr auto as_u(i64 x) -> u64 { return static_cast<u64>(x); }
constexpr auto as_u(i32 x) -> u32 { return static_cast<u32>(x); }
constexpr auto as_u(i8 x) -> u8 { return static_cast<u8>(x); }

constexpr auto encode_upper_bound(u64 const&) -> u32 { return 8; }
constexpr auto encode_upper_bound(i64 const&) -> u32 { return 8; }
constexpr auto encode_upper_bound(u32 const&) -> u32 { return 4; }
constexpr auto encode_upper_bound(i32 const&) -> u32 { return 4; }
constexpr auto encode_upper_bound(u8 const&) -> u32 { return 1; }
constexpr auto encode_upper_bound(i8 const&) -> u32 { return 1; }

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

}
