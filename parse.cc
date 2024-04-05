#include "common.hh"
#include "array.hh"

#define tail [[clang::musttail]] return

using std::forward;

namespace lang {

namespace {

constexpr struct None {} none;

template <class T>
bool operator==(Span<T> const& a, Span<T> const& b) {
  if (len(a) != len(b))
    return false;
  return !memcmp(a.base, b.base, len(a) * sizeof(T));
}

template <class T>
T const& last(List<T> const& list) {
  return list[len(list) - 1];
}

template <class T>
T& last(List<T>& list) {
  return list[len(list) - 1];
}

struct MaybeU32 {
  u32 value;
  MaybeU32() = default;
  MaybeU32(u32 x): value(x + 1) {}
  MaybeU32(None): value() {}
  explicit operator bool() const { return value; }
  u32 operator*() const { return value - 1; }
};

template <class T>
MaybeU32 find(ArraySpan<T> const& list, Span<T> item) {
  u32 n = len(list);
  for (u32 i {}; i < n; ++i) {
    auto opt = list[i];
    if (len(opt) == len(item) && !memcmp(item.begin(), opt.begin(), sizeof(T) * len(opt)))
      return i;
  }
  return none;
}

Str str_between(char const* begin, char const* end) {
  check(begin <= end);
  return {begin, u32(end - begin)};
}

bool is_alphanum(char c) {
  return u32(c - 'a') < 26 || u32(c - 'A') < 26 || u32(c - '0') < 10;
}

bool is_name_char(char c) {
  return u32(c - 'a') < 26 || u32(c - 'A') < 26 || u32(c - '0') < 10 || c == '_';
}

template <class T>
struct ArrayArrayList {
  ArrayList<T> list;
  List<u32> ofs;

  ArrayArrayList() { ofs.push(0); }

  void push_empty() {
    ofs.push(last(ofs));
  }

  void last_push(Span<T> items) {
    list.push(items);
    ++last(ofs);
  }

  friend u32 len(ArrayArrayList const& list) { return list.ofs.size - 1; }
  friend ArraySpan<T> last(ArrayArrayList<T> const& list) {
    auto& ofs = list.ofs;
    auto end = ofs[len(ofs) - 1];
    auto begin = ofs[len(ofs) - 2];
    return {list.list.list.begin(), list.list.ofs.begin() + begin, end - begin};
  }
};

using StrArrayList = ArrayArrayList<char>;

struct Parser {
  List<u32> indent {};
  enum { Struct, Union } cur_decl {};

  ArrayList<char> types;
  StrArrayList struct_members;

  Parser() {
    indent.push(0);
    types.push("u8"_s);
    types.push("u16"_s);
    types.push("u32"_s);
    types.push("u64"_s);
    types.push("i8"_s);
    types.push("i16"_s);
    types.push("i32"_s);
    types.push("i64"_s);
    types.push("f32"_s);
  }

  template <class... T>
  void fail(T&&... args) {
    println("error: "_s, forward<T>(args)...);
    abort();
  }

  void (Parser::*next)(char const*) = &Parser::decl;

  void spaces(char const*& it) {
    while (*it == ' ')
      ++it;
  }

  bool literal(char const*& it, char const* word) {
    auto cp = it;
    while (*word) {
      if (*cp++ != *word++)
        return false;
    }
    it = cp;
    return true;
  }

  Str word(char const*& it) {
    auto begin = it;
    check(is_name_char(*it));
    do
      ++it;
    while (is_name_char(*it));
    return str_between(begin, it);
  }

  void member(char const* it) {
    auto name = word(it);
    spaces(it);

    Str array_size {};
    if (*it == '[') {
      auto begin = ++it;
      check(*it != ']');
      do {
        ++it;
        check(*it != '\n' && *it != '\0');
      } while (*it != ']');
      array_size = str_between(begin, it++);
      spaces(it);
    }
    auto type_name = word(it);
    auto type = find_type(type_name);
    check(!!type);
    spaces(it);
    Str type_index_name {};
    MaybeU32 type_index {};
    if (*it == '[') {
      auto begin = ++it;
      check(*it != ']');
      do {
        ++it;
        check(*it != '\n' && *it != '\0');
      } while (*it != ']');
      type_index_name = str_between(begin, it++);
      type_index = find(last(struct_members), type_index_name);
      spaces(it);
    }
    check(*it == '\n');
    struct_members.last_push(name);
    if (type_index) {
      println(":member "_s, name, ' ', type_name, '(', *type, ")["_s, type_index_name, '(', *type_index, ")] "_s, array_size);
    } else {
      println(":member "_s, name, ' ', type_name, '(', *type, ") "_s, array_size);
    }
    tail start_line(it + 1);
  }

  void union_member(char const* it) {
    auto type_name = word(it);
    auto type = find_type(type_name);
    check(!!type);
    spaces(it);
    check(*it == '\n');
    println(":variant "_s, type_name, '(', *type, ") "_s);
    tail start_line(it + 1);
  }

  MaybeU32 find_type(Str name) {
    return find(types.span(), name);
  }

  void struct_(char const* it) {
    cur_decl = Struct;
    struct_members.push_empty();
    while (*it == ' ')
      ++it;
    auto name_begin = it;
    while (is_alphanum(*it))
      ++it;
    auto cur_struct = str_between(name_begin, it);
    println(":struct "_s, cur_struct);
    if (find_type(cur_struct))
      return fail("redefinition of "_s, cur_struct);
    types.push(cur_struct);
    while (*it == ' ')
      ++it;
    check(*it == '\n');
    tail start_line(it + 1);
  }

  void union_(char const* it) {
    cur_decl = Union;
    while (*it == ' ')
      ++it;
    auto name_begin = it;
    while (is_alphanum(*it))
      ++it;
    auto name = str_between(name_begin, it);
    println(":union "_s, name);
    if (find_type(name))
      return fail("redefinition of "_s, name);
    types.push(name);
    while (*it == ' ')
      ++it;
    check(*it == '\n');
    tail start_line(it + 1);
  }

  void decl(char const* it) {
    auto token = word(it);
    if (token == "struct"_s)
      tail struct_(it);
    if (token == "union"_s)
      tail union_(it);
    return fail("unrecognized declaration '"_s, token, '\'');
  }

  void start_line(char const* it) {
    u32 n {};
    while (*it == ' ') {
      ++it;
      ++n;
    }
    if (*it == '\n')
      tail start_line(it + 1);
    if (*it == '\0')
      return;

    if (n > last(indent)) {
      println(":pushScope"_s);
      switch (cur_decl) {
        case Struct:
          next = &Parser::member;
          break;
        case Union:
          next = &Parser::union_member;
          break;
      }
      indent.push(n);
    } else {
      while (n < last(indent)) {
        indent.pop();
        println(":popScope"_s);
        next = &Parser::decl;  // pop scope
      }
      check(n == last(indent));
    }

    tail (this->*next)(it);
  }
};

struct TypeWriter {
  char* it;
  void name(Str x) {
    check(len(x) < 256);
    *it++ = char(u8(len(x)));
    memcpy(it, x.begin(), len(x));
    it += len(x);
  }
};

struct Type {
  char const* it;
  Str name() {
    u32 size = u32(*it++);
    return {exchange(it, it + size), size};
  }
};

void test_roundtrip(Str s) {
  Parser p {};
  p.start_line(s.begin());
}

}

void parse() {
  println("parse "_s);
  test_roundtrip(R"(
struct DroppedGpsMessage
  count u16
  reason[count] u8

struct BadIslLength
  length u32

struct BadIslTime
  gps_ms u64

struct RanDod
  abs_mean[6] f32
  rel_mean[6] f32
  amb_count u32
  amb_sd[amb_count] i8
  amb_prn[amb_count] u8

union LogType
  DroppedGpsMessage
  BadIslLength
  BadIslTime
  RanDod

struct Log
  type u32
  message LogType[type]
  )"_s);
}

}

