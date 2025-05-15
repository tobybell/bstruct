#include "parse.hh"

#include "common.hh"
#include "array.hh"

#define tail [[clang::musttail]] return
#define unreachable abort()

template <class T>
void last_push(ArrayList<T>& list, T const& x) {
  list.list.push(x);
  ++list.ofs[len(list.ofs) - 1];
}

using uptr = unsigned long;
static_assert(sizeof(uptr) == sizeof(void*));

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

template <class T, class I>
Span<T> array_span_get(T* value, I const* end, I first, u32 i) {
  auto begin = i ? end[i - 1] : first;
  return {value + begin, u32(end[i] - begin)};
}

template <class T, class I>
MaybeU32 array_span_find(T const* value, I const* end, I first, u32 n, Span<T> item) {
  for (u32 i {}; i < n; ++i) {
    auto opt = array_span_get(value, end, first, i);
    if (len(opt) == len(item) && !memcmp(item.begin(), opt.begin(), sizeof(T) * len(item)))
      return i;
  }
  return none;
}

template <class T>
MaybeU32 find(ArraySpan<T> const& list, Span<T> item) {
  return array_span_find(list.base, list.ofs.begin(), list.first, len(list), item);
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

  void push_empty() {
    ofs.push(ofs ? last(ofs) : 0);
  }

  void last_push(Span<T> items) {
    list.push(items);
    ++last(ofs);
  }

  friend u32 len(ArrayArrayList const& list) { return list.ofs.size; }
  ArraySpan<T> operator[](u32 i) const {
    auto end = ofs[i];
    auto begin = i ? ofs[i - 1] : 0;
    return {list.list.begin(), {list.ofs.begin() + begin, end - begin}, begin ? list.ofs[begin - 1] : 0};
  }
  friend ArraySpan<T> last(ArrayArrayList<T> const& list) {
    return list[len(list) - 1];
  }
};

using StrList = ArrayList<char>;
using StrArrayList = ArrayArrayList<char>;

struct Member {
  u32 type;
  ArrayType array;
  u32 length;
};

constexpr u8 PrimitiveSize[PrimitiveCount] {1, 2, 4, 8, 1, 2, 4, 8, 4, 8};
constexpr char PrimitiveName[] = "u8u16u32u64i8i16i32i64f32f64";
constexpr u8 PrimitiveNameEnd[PrimitiveCount] {2, 5, 8, 11, 13, 16, 19, 22, 25, 28};

struct Parser {
  List<u32> indent {};
  enum { Struct, Log } cur_decl {};

  StrList type_names;
  List<u32> struct_type;
  StrArrayList struct_member_name;
  ArrayList<Member> struct_member;
  List<u32> log_type;
  ArrayList<u32> log_member_struct;

  Str get_type_name(u32 i) const {
    if (i < PrimitiveCount)
      return primitive_name(PrimitiveId(i));
    return type_names[i - PrimitiveCount];
  }

  Parser() {
    indent.push(0);
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
    last_push(struct_member, member);
    tail start_line(it + 1);
  }

  void log_member(char const* it) {
    auto type_name = word(it);
    auto type = find_type(type_name);
    check(!!type);
    spaces(it);
    check(*it == '\n');
    auto struct_ = find(struct_type.span(), *type);
    last_push(log_member_struct, *struct_);
    tail start_line(it + 1);
  }

  MaybeU32 find_type(Str name) {
    auto i = find(span(type_names), name);
    if (!i)
      return array_span_find(PrimitiveName, PrimitiveNameEnd, u8(0), PrimitiveCount, name);
    return *i + PrimitiveCount;
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
    u32 type = len(type_names) + PrimitiveCount;
    type_names.push(cur_struct);
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
    auto type = PrimitiveCount + len(type_names);
    type_names.push(name);
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

void print_to_bstruct(Parser const& p, Print& s) {
  for (auto struct_: range(len(p.struct_type))) {
    auto type = p.struct_type[struct_];
    auto member_name = p.struct_member_name[struct_];
    auto member_info = p.struct_member[struct_];

    sprint(s, "struct "_s, p.get_type_name(type), '\n');
    for (auto member: range(len(member_info))) {
      auto name = member_name[member];
      auto info = member_info[member];
      sprint(s, "  "_s, name);
      if (info.array == FixedArray)
        sprint(s, '[', info.length, ']');
      else if (info.array == MemberArray)
        sprint(s, '[', member_name[info.length], ']');
      else if (info.array != NoArray)
        unreachable;
      sprint(s, ' ', p.get_type_name(info.type), '\n');
    }
    sprint(s, '\n');
  }

  auto n_log = len(p.log_type);
  for (auto log: range(n_log)) {
    auto type = p.log_type[log];
    auto member_struct = p.log_member_struct[log];
    sprint(s, "log "_s, p.get_type_name(type), '\n');
    for (u32 member: range(len(member_struct)))
      sprint(s, "  "_s, p.get_type_name(p.struct_type[member_struct[member]]), '\n');
  }
}

void test_roundtrip(Str s) {
  Parser p {};
  p.start_line(s.begin());
  Print buf;
  print_to_bstruct(p, buf);
  check(len(buf.chars) == len(s));
  check(!memcmp(buf.chars.begin(), s.base, len(buf.chars)));
}

PrimitiveId type_primitive(u32 type) {
  check(type < PrimitiveCount);
  return PrimitiveId(type);
}

void write_member(Stream& s, LibraryStruct, LibraryMember m, u8 arg) {
  switch (type_primitive(m.type)) {
    case U8:
      memcpy(s.reserve(1), &arg, 1);
      s.size += 1;
      break;
    case U32: {
      u32 x = arg;
      memcpy(s.reserve(4), &x, 4);
      s.size += 4;
      break;
    }
    default:
      unreachable;
  }
}

u32 read_u32(char const* i, PrimitiveId t) {
  switch (t) {
    case U8: return u32(*(u8 const*) i);
    case U32: return *(u32 const*) i;
    default: unreachable;
  }
}

u32 get_field(char const* base, LibraryStruct s, u32 member) {
  check(member < s.memberCount);
  u32 ofs {};
  for (u32 i {}; i < member; ++i) {
    ofs += PrimitiveSize[type_primitive(s.member[i].type)];
  }
  return read_u32(base + ofs, type_primitive(s.member[member].type));
}

uptr array_length(Stream& s, LibraryStruct st, LibraryMember m) {
  if (m.array == FixedArray)
    return m.length;
  if (m.array == MemberArray)
    return get_field(s.begin(), st, m.length);
  unreachable;
}

template <u32 N>
void write_member(Stream& s, LibraryStruct st, LibraryMember m, u8 const (&arg)[N]) {
  check(type_primitive(m.type) == U8);
  check(array_length(s, st, m) == N);
  memcpy(s.reserve(N), &arg, N);
  s.size += N;
}

template <class... T>
String make_struct(LibraryStruct s, T&&... args) {
  Stream out;
  auto m = s.member;
  (write_member(out, s, *m++, forward<T>(args)),...);
  return out.take();
};

template <class T>
char const* print_primitive(Print& p, char const* it) {
  sprint(p, *(T const*) it);
  return it + sizeof(T);
}

char const* (*basic_type_printer(PrimitiveId t))(Print& p, char const* it) {
  switch (t) {
    case U8: return print_primitive<u8>;
    case U16: return print_primitive<u16>;
    case U32: return print_primitive<u32>;
    case U64: return print_primitive<u64>;
    case I8: return print_primitive<i8>;
    case I16: return print_primitive<i16>;
    case I32: return print_primitive<i32>;
    case I64: return print_primitive<i64>;
    case F32: return print_primitive<f32>;
    case F64: return print_primitive<f64>;
    default: unreachable;
  }
}

char const* print_primitive(Print& p, PrimitiveId t, char const* it) {
  return basic_type_printer(t)(p, it);
}

char const* print_custom_type(Print& p, Library const& l, u32 t, char const* it);

char const* print_array(Print& p, PrimitiveId t, u32 count, char const* it) {
  sprint(p, '[');
  auto printer = basic_type_printer(t);
  if (count) {
    it = printer(p, it);
    for (u32 i = 1; i < count; ++i) {
      sprint(p, ' ');
      it = printer(p, it);
    }
  }
  sprint(p, ']');
  return it;
}

char const* print_value(Print& p, Library const& l, LibraryStruct st, u32 type, ArrayType arr, u32 length, char const* it, char const* begin) {
  if (arr == NoArray) {
    if (type < PrimitiveCount)
      return print_primitive(p, type_primitive(type), it);
    return print_custom_type(p, l, type - PrimitiveCount, it);
  } else if (arr == FixedArray) {
    u32 real_length = length;
    return print_array(p, type_primitive(type), real_length, it);
  } else if (arr == MemberArray) {
    u32 real_length = get_field(begin, st, length);
    return print_array(p, type_primitive(type), real_length, it);
  } else
    unreachable;
}

char const* print_member(Print& p, Library const& l, LibraryStruct st, LibraryMember s, char const* it, char const* struct_begin) {
  sprint(p, ' ', l.names[s.name], '=');
  return print_value(p, l, st, s.type, s.array, s.length, it, struct_begin);
}

char const* print_struct(Print& p, Library const& l, LibraryStruct s, char const* it) {
  sprint(p, l.names[s.name]);
  auto struct_begin = it;
  for (u32 i: range(s.memberCount)) {
    it = print_member(p, l, s, s.member[i], it, struct_begin);
  }
  return it;
}

void print_struct(Print& p, Library const& l, LibraryStruct s, Str b) {
  print_struct(p, l, s, b.begin());
}

char const* print_custom_type(Print& p, Library const& l, u32 t, char const* it) {
  return print_struct(p, l, l.type(t), it);
}

u32 find_or_add(ArrayList<char>& strs, Str str) {
  auto exist = find(span(strs), str);
  if (exist)
    return *exist;
  auto id = len(strs);
  strs.push(str);
  return id;
}

u8 operator""_u8(unsigned long long x) {
  check(x <= 255);
  return u8(x);
}

void test_print_value() {
  auto types = parse(R"(struct Person
  age u8
  weight u8
  )"_s);
  auto personType = types.type("Person"_s);
  auto data = make_struct(personType, 27_u8, 150_u8);
  check(data.span() == "\33\226"_s);

  Print p;
  print_struct(p, types, personType, data);
  check(p.chars.span() == "Person age=27 weight=150"_s);
}

void test_print_value_array() {
  auto types = parse(R"(struct Person
  name_len u32
  name[name_len] u8
  )"_s);
  auto personType = types.type("Person"_s);
  auto data = make_struct(personType, 5_u8, (u8 const(&)[5]) "AAAAA");
  check(data.span() == "\5\0\0\0\101\101\101\101\101"_s);

  Print p;
  print_struct(p, types, personType, data);
  check(p.chars.span() == "Person name_len=5 name=[65 65 65 65 65]"_s);
}

}

void parse() {
  test_roundtrip(R"(struct DroppedGpsMessage
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
  test_print_value();
  test_print_value_array();
  println("Parse tests passed");
}

Library parse(Str schema) {
  Parser p {};
  p.start_line(schema.begin());

  ArrayList<char> names;
  List<u32> struct_names;
  ArrayList<LibraryMember> members;
  for (auto struct_: range(len(p.struct_type))) {
    auto type = p.struct_type[struct_];
    auto name = p.get_type_name(type);
    auto name_id = find_or_add(names, name);
    struct_names.push(name_id);

    auto member_name = p.struct_member_name[struct_];
    auto member_info = p.struct_member[struct_];
    members.push_empty(0);
    for (auto member: range(len(member_info))) {
      auto name = member_name[member];
      auto name_id = find_or_add(names, name);
      auto info = member_info[member];
      last_push(members, LibraryMember {name_id, info.type, info.array, info.length});
    }
  }
  Library ans;
  ans.names = names.take();
  ans.struct_names = struct_names.take();
  ans.struct_member = members.take();
  
  return ans;
}

void print_to_bstruct(Library const& p, Print& s) {
  for (auto struct_: p.structs()) {
    sprint(s, "struct "_s, struct_.name(), '\n');
    for (auto member: struct_.members()) {
      sprint(s, "  "_s, member.name());
      if (member.fixed_array())
        sprint(s, '[', member.length_fixed(), ']');
      else if (member.member_array())
        sprint(s, '[', member.length_member().name(), ']');
      else
        check(member.no_array());
      sprint(s, ' ', member.type().name(), '\n');
    }
    sprint(s, '\n');
  }
  s.chars.pop();
}

u32 primitive_size(PrimitiveId p) { return PrimitiveSize[p]; }

Str primitive_name(PrimitiveId i) {
  u32 begin = i ? u32(PrimitiveNameEnd[i - 1]) : 0u;
  return {PrimitiveName + begin, u32(PrimitiveNameEnd[i]) - begin};
}
