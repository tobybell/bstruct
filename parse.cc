#include "common.hh"
#include "array.hh"

#define tail [[clang::musttail]] return

using std::forward;

namespace lang {

namespace {

template <class T>
T const& last(List<T> const& list) {
  return list[len(list) - 1];
}

struct MaybeU32 {
  u32 value;
  explicit operator bool() const { return value; }
  u32 operator*() const { return value - 1; }
  static MaybeU32 from(u32 x) { return {x + 1}; }
};

template <class T>
MaybeU32 find(ArrayList<T> const& list, Span<T> item) {
  u32 n = len(list);
  for (u32 i {}; i < n; ++i) {
    auto opt = list[i];
    if (len(opt) == len(item) && !memcmp(item.begin(), opt.begin(), sizeof(T) * len(opt)))
      return MaybeU32::from(i);
  }
  return {};
}

Str str_between(char const* begin, char const* end) {
  check(begin <= end);
  return {begin, u32(end - begin)};
}

bool is_alphanum(char c) {
  return u32(c - 'a') < 26 || u32(c - 'A') < 26 || u32(c - '0') < 10;
}

struct Parser {
  List<u32> indent {};

  ArrayList<char> types;

  Str cur_struct;

  Parser() {
    indent.push(0);
    types.push("u32"_s);
    types.push("u8"_s);
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

  Str word(char const*& it) {
    auto begin = it;
    check(is_alphanum(*it));
    do
      ++it;
    while (is_alphanum(*it));
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
    auto type = find(types, type_name);
    check(!!type);
    spaces(it);
    check(*it == '\n');
    println(":member "_s, name, ' ', type_name, '(', *type, ") "_s, array_size);
    tail start_line(it + 1);
  }

  void decl(char const* it) {
    check(*it++ == 's');
    check(*it++ == 't');
    check(*it++ == 'r');
    check(*it++ == 'u');
    check(*it++ == 'c');
    check(*it++ == 't');
    while (*it == ' ')
      ++it;
    auto name_begin = it;
    while (is_alphanum(*it))
      ++it;
    cur_struct = str_between(name_begin, it);
    println(":struct "_s, cur_struct);
    if (find(types, cur_struct))
      return fail("redefinition of "_s, cur_struct);
    types.push(cur_struct);
    while (*it == ' ')
      ++it;
    check(*it == '\n');
    tail start_line(it + 1);
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
      next = &Parser::member;  // push scope
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

void test_roundtrip(Str s) {
  Parser p {};
  p.start_line(s.begin());
}

}

void parse() {
  println("parse "_s);
  test_roundtrip(R"(
struct Str
  count u32
  data[count] u8
struct Str2
  count u32
  data[count] u8
  other Str
  )"_s);
}

}

