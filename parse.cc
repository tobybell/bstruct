#include "common.hh"
#include "array.hh"

#define tail [[clang::musttail]] return
#define unreachable abort()

using std::forward;

namespace lang {

namespace {

u32 parse_u32(Str s) {
  u32 ans {};
  for (char c: s) {
    check(u32(c - '0') < 10);
    ans = ans * 10 + u32(c - '0');
  }
  return ans;
}

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

template <class T>
MaybeU32 find(Span<T> const& list, T const& item) {
  u32 n = len(list);
  for (u32 i {}; i < n; ++i) {
    if (list[i] == item)
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
  ArraySpan<T> operator[](u32 i) const {
    auto end = ofs[i + 1];
    auto begin = ofs[i];
    return {list.list.begin(), {list.ofs.begin() + begin, ++end - begin}};
  }
  friend ArraySpan<T> last(ArrayArrayList<T> const& list) {
    return list[len(list) - 1];
  }
};

using StrList = ArrayList<char>;
using StrArrayList = ArrayArrayList<char>;

enum ArrayType {
  NoArray,
  FixedArray,
  MemberArray
};

struct Member {
  u32 type;
  ArrayType array;
  u32 length;
};

struct Parser {
  List<u32> indent {};
  enum { Struct, Log } cur_decl {};

  StrList types;
  List<u32> struct_type;
  StrArrayList struct_member_name;
  ArrayList<Member> struct_member;
  List<u32> log_type;
  ArrayList<u32> log_member_struct;

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
    check(*it == '\n');
    Member member {};
    member.type = *type;
    if (array_size) {
      auto length_member = find(last(struct_member_name), array_size);
      if (length_member) {
        member.array = MemberArray;
        member.length = *length_member;
      } else {
        member.array = FixedArray;
        member.length = parse_u32(array_size);
      }
    }
    struct_member_name.last_push(name);
    struct_member.last_push(member);
    tail start_line(it + 1);
  }

  void log_member(char const* it) {
    auto type_name = word(it);
    auto type = find_type(type_name);
    check(!!type);
    spaces(it);
    check(*it == '\n');
    auto struct_ = find(struct_type.span(), *type);
    log_member_struct.last_push(*struct_);
    tail start_line(it + 1);
  }

  MaybeU32 find_type(Str name) {
    return find(types.span(), name);
  }

  void struct_(char const* it) {
    cur_decl = Struct;
    while (*it == ' ')
      ++it;
    auto name_begin = it;
    while (is_alphanum(*it))
      ++it;
    auto cur_struct = str_between(name_begin, it);
    if (find_type(cur_struct))
      return fail("redefinition of "_s, cur_struct);
    u32 type = len(types);
    types.push(cur_struct);
    struct_type.push(type);
    struct_member_name.push_empty();
    struct_member.push_empty(0);
    while (*it == ' ')
      ++it;
    check(*it == '\n');
    tail start_line(it + 1);
  }

  void log(char const* it) {
    cur_decl = Log;
    while (*it == ' ')
      ++it;
    auto name_begin = it;
    while (is_alphanum(*it))
      ++it;
    auto name = str_between(name_begin, it);
    if (find_type(name))
      return fail("redefinition of "_s, name);
    auto type = len(types);
    types.push(name);
    log_type.push(type);
    log_member_struct.push_empty(0);
    while (*it == ' ')
      ++it;
    check(*it == '\n');
    tail start_line(it + 1);
  }

  void decl(char const* it) {
    auto token = word(it);
    if (token == "struct"_s)
      tail struct_(it);
    if (token == "log"_s)
      tail log(it);
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
      switch (cur_decl) {
        case Struct:
          next = &Parser::member;
          break;
        case Log:
          next = &Parser::log_member;
          break;
      }
      indent.push(n);
    } else {
      while (n < last(indent)) {
        indent.pop();
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
  for (auto struct_: range(len(p.struct_type))) {
    auto type = p.struct_type[struct_];
    auto member_name = p.struct_member_name[struct_];
    auto member_info = p.struct_member[struct_];
    println("struct "_s, p.types[type], " {"_s);
    for (auto member: range(len(member_info))) {
      auto name = member_name[member];
      auto info = member_info[member];
      if (info.array == NoArray)
        println("  "_s, p.types[info.type], ' ', name, ';');
      else if (info.array == FixedArray)
        println("  "_s, p.types[info.type], ' ', name, '[', info.length, "];"_s);
      else if (info.array == MemberArray) {
        println("  "_s, p.types[info.type], " const* "_s, name, ';');
      } else
        unreachable;
    }
    println("};\n"_s);
  }

  auto n_log = len(p.log_type);
  for (auto log: range(n_log)) {
    auto type = p.log_type[log];
    auto member_struct = p.log_member_struct[log];
    Stream s;
    sprint(s, "struct "_s, p.types[type], ": Log<"_s);
    sprint(s, p.types[p.struct_type[member_struct[0]]]);
    for (auto member: range(1, len(member_struct)))
      sprint(s, ", "_s, p.types[p.struct_type[member_struct[member]]]);
    sprint(s, "> {};"_s);
    println(s.str(), '\n');
  }
}

}

void parse() {
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

log LogType
  DroppedGpsMessage
  BadIslLength
  BadIslTime
  RanDod
)"_s);
}

}

