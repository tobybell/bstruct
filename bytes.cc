#include "bytes.hh"

#include <cstdlib>
#include <cstring>

namespace lang {

static auto ceil2(u64 x) -> u64 {
  return 2u << (31 - __builtin_clzll(x - 1));
}

auto grow(Bytes& v, u64 new_size) -> void {
  if (v.capacity >= new_size)
    return;
  auto new_capacity = ceil2(new_size);
  v.data = (u8*) realloc(v.data, new_capacity);
  v.capacity = new_capacity;
}

auto push_from(Bytes& v, u8 const* data, u64 size) -> void {
  auto cur_size = v.size;
  auto new_size = cur_size + size;
  grow(v, new_size);
  ::memcpy(&v[cur_size], data, size);
  v.size = new_size;
}

}
