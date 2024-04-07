#pragma once

#include "common.hh"

#include <type_traits>
#include <utility>

namespace lang {

template <class T>
constexpr bool is_trivial = std::is_trivial_v<T>;

template <class T>
struct Array {
  static_assert(is_trivial<T>);
  T* data {};
  u32 size {};
  Array() {}
  explicit Array(u32 size_): data((T*) malloc(size_ * sizeof(T))), size(size_) {}
  Array(Span<T> items): data(malloc(items.size * sizeof(T))), size(items.size) {
    memcpy(data, items.data, items.size * sizeof(T));
  }
  Array(Array const& rhs): Array(rhs.span()) {}
  ~Array() { free(data); }
  void resize(u32 new_size) {
    data = (T*) realloc(data, new_size * sizeof(T));
    size = new_size;
  }
  T& operator[](u32 index) { return data[index]; }
  T const& operator[](u32 index) const { return data[index]; }
  friend u32 len(Array const& array) { return array.size; }
  T const* begin() const { return data; }
  Span<T> span() const { return {data, size}; }
};

template <class T>
struct List {
  static_assert(is_trivial<T>);
  Array<T> array {};
  u32 size {};
  T& operator[](u32 index) { return array[index]; }
  T const& operator[](u32 index) const { return array[index]; }
  friend u32 len(List const& array) { return array.size; }
  friend T& last(List& x) { return x[len(x) - 1]; }
  T const* begin() const { return array.begin(); }
  T const* end() const { return begin() + size; }
  operator Span<T>() const { return {begin(), size}; }
  void push(T const& item) {
    if (size == array.size)
      array.resize(size ? size * 2 : 4);
    array[size++] = item;
  }
  void pop() {
    check(size);
    --size;
  }
  void expand(u32 needed) {
    u32 capacity = array.size;
    if (needed <= capacity)
      return;
    if (!capacity)
      capacity = needed;
    else
      while (capacity < needed)
        capacity *= 2;
    array.resize(capacity);
  }
};

template <class T>
struct Mut {
  T* base;
  u32 size;
  T* begin() const { return base; }
  T* end() const { return base + size; }
  T& operator[](u32 index) const { return base[index]; }
  friend u32 len(Mut const& x) { return x.size; }
  explicit operator bool() const { return size; }
  Span<T> span() const { return {base, size}; }
  operator Span<T>() const { return {base, size}; }
};

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
struct ArrayList {
  List<T> list;
  List<u32> ofs;

  ArrayList() { ofs.push(0); }
  ArrayList(List<T> list_, List<u32> ofs_):
    list(std::move(list_)), ofs(std::move(ofs_)) {}

  Mut<T> push_empty(u32 size) {
    auto orig_size = list.size;
    list.expand(list.size + size);
    list.size += size;
    ofs.push(list.size);
    return {&list[orig_size], size};
  }

  void last_push(T const& item) {
    list.push(item);
    ++last(ofs);
  }

  void push(Span<T> items) {
    auto storage = push_empty(len(items));
    memcpy(storage.begin(), items.begin(), sizeof(T) * len(items));
  }

  void pop() {
    ofs.pop();
    list.size = ofs[ofs.size - 1];
  }

  friend u32 len(ArrayList const& list) { return list.ofs.size - 1; }
  Mut<T> operator[](u32 index) {
    return {&list[ofs[index]], ofs[index + 1] - ofs[index]};
  }
  Span<T> operator[](u32 index) const {
    return {&list[ofs[index]], ofs[index + 1] - ofs[index]};
  }
  ArraySpan<T> span() const { return {list.begin(), ofs}; }
  operator ArraySpan<T>() const { return span(); }
};

}
