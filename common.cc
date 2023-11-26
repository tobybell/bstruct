#include "common.hh"

#include <algorithm>

namespace lang {

void print(u32 x, Stream& s) {
  s.reserve(10);
  auto it = &s.data[s.size];
  do {
    *it++ = '0' + (x % 10);
    x /= 10;
  } while (x);
  std::reverse(&s.data[s.size], it);
  s.size = u32(it - s.data);
}

}

