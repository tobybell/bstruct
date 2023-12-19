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

void print(u64 x, Stream& s) {
  s.reserve(20);
  auto it = &s.data[s.size];
  do {
    *it++ = '0' + (x % 10);
    x /= 10;
  } while (x);
  std::reverse(&s.data[s.size], it);
  s.size = u32(it - s.data);
}

void print(char x, Stream& s) {
  s.reserve(1);
  s.data[s.size++] = x;
}

void print(char const* x, Stream& s) {
  for (; *x; ++x)
    print(*x, s);
}

}

