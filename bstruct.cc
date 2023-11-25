#include "backend.hh"

#include <cstdlib>
#include <unistd.h>
#include <type_traits>
#include <algorithm>

using namespace lang;

void check(bool condition) {
  if (!condition)
    abort();
}

char buf[4096 * 4096];

template <class T>
constexpr bool is_trivial = std::is_trivial_v<T>;

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

  void push(Span<T> items) {
    auto storage = push_empty(len(items));
    std::copy(items.begin(), items.end(), storage.begin());
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
};


using String = Array<char>;
using StringList = ArrayList<char>;

enum Builtin { U32 };

struct Struct {
  StringList names;
  List<u32> array_size;
  void field(Span<char> name) {
    names.push(name);
    array_size.push(0);
  }
  void field(Span<char> name, u32 size) {
    names.push(name);
    array_size.push(size + 1);
  }
};


// assumes 10 bytes free
u32 to_string(char* out, u32 n) {
  u32 i = 0;
  do {
    out[i++] = '0' + (n % 10);
    n /= 10;
  } while (n);
  std::reverse(out, out + i);
  return i;
}

char const* print_u32(char const* i) {
  char i_buf[10];
  ::write(1, i_buf, to_string(i_buf, *(u32 const*) i));
  return i + 4;
}

void test_case(Struct& s, Span<char> data) {
  char const* it = data.base;
  for (u32 i = 0; i < len(s.names); ++i) {
    auto name = s.names[i];
    ::write(1, name.base, name.size);
    ::write(1, ": ", 2);

    u32 arr_sz = s.array_size[i];
    if (arr_sz) {
      u32 array_size = arr_sz - 1;
      ::write(1, "[", 1);
      if (array_size) {
        it = print_u32(it);
        for (u32 j = 1; j < array_size; ++j) {
          ::write(1, ", ", 2);
          it = print_u32(it);
        }
      }
      ::write(1, "]", 1);
    } else {
      it = print_u32(it);
    }
    ::write(1, "\n", 1);
  }
}

int main() {

  {
    lang::Backend b;
    b.mov(rax, 49);
    b.ret();
    try_running_it(b.output);
  }

  {
    Struct s;
    s.field("name"_s);
    s.field("age"_s);
    s.field("skill"_s);
    s.field("health"_s);
    u32 nums[4] {1,2,3,4};
    test_case(s, {(char const*) nums, 16});
  }

  {
    Struct s;
    s.field("name"_s);
    s.field("age"_s, 3);
    u32 nums[4] {1,2,3,4};
    test_case(s, {(char const*) nums, 16});
  }

  return 0;

  auto n = ::read(0, buf, sizeof(buf));
  check(n >= 0);
  check(::write(1, buf, u32(n)) == n);
}
