#pragma once

#include "common.hh"

template <class T>
struct ArraySpan {
  T const* base;
  Span<u32> ofs;
  u32 first = 0;

  friend u32 len(ArraySpan const& x) { return len(x.ofs); }
  Span<T> operator[](u32 i) const {
    auto begin = i ? ofs[i - 1] : first;
    return {base + begin, ofs[i] - begin};
  }

  struct Iterator {
    T const* base;
    u32 const* ofs;
    u32 first;
    Span<T> operator*() const { return {base + first, *ofs - first}; }
    void operator++() { first = *ofs; ++ofs; }
    bool operator!=(Iterator const& rhs) const { return ofs != rhs.ofs; }
  };

  Iterator begin() const { return {base, ofs.begin(), first}; }
  Iterator end() const { return {base, ofs.end()}; }
};

template <class T>
ArraySpan<T> span(ArrayList<T> const& x) {
  return {x.list.begin(), x.ofs};
}
