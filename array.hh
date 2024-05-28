#pragma once

#include "common.hh"

template <class T>
struct ArraySpan {
  T const* base;
  Span<u32> ofs;
  friend u32 len(ArraySpan const& x) { return len(x.ofs) - 1; }
  Span<T> operator[](u32 i) const { return {base + ofs[i], ofs[i + 1] - ofs[i]}; }

  struct Iterator {
    T const* base;
    u32 const* ofs;
    Span<T> operator*() const { return {base + *ofs, ofs[1] - *ofs}; };
    void operator++() { ++ofs; }
    bool operator!=(Iterator const& rhs) const { return ofs != rhs.ofs; }
  };

  Iterator begin() const { return {base, ofs.begin()}; }
  Iterator end() const { return {base, ofs.end() - 1}; }
};

template <class T>
ArraySpan<T> span(ArrayList<T> const& x) {
  return {x.list.begin(), x.ofs};
}
