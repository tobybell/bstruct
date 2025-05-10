#pragma once

#include "common.hh"

enum PrimitiveId: u8 {
  U8, U16, U32, U64, I8, I16, I32, I64, F32, F64, PrimitiveCount
};

Str primitive_name(PrimitiveId);
u32 primitive_size(PrimitiveId);

enum ArrayType {
  NoArray,
  FixedArray,
  MemberArray
};

struct LibraryMember {
  u32 name;
  u32 type;
  ArrayType array;
  u32 length;
};

struct LibraryStruct {
  u32 name;
  u32 memberCount;
  LibraryMember const* member;
};

struct Primitive {
  PrimitiveId id;
  Str name() const { return primitive_name(id); }
  u32 size() const { return primitive_size(id); }
};

struct Library {
  ArrayArray<char> names;
  Array<u32> struct_names;
  ArrayArray<LibraryMember> struct_member;

  struct Type;

  struct Member {
    Library const& l;
    u32 i;
    u32 struct_base;
    LibraryMember const& library_member() const {
      return l.struct_member.items[i];
    }
    Str name() const { return l.names[library_member().name]; }
    ArrayType array_type() const { return library_member().array; }
    bool fixed_array() const { return array_type() == FixedArray; }
    bool member_array() const { return array_type() == MemberArray; }
    bool no_array() const { return array_type() == NoArray; }
    u32 length_fixed() const {
      check(fixed_array());
      return library_member().length;
    }
    Member length_member() const {
      check(member_array());
      return {l, struct_base + library_member().length, struct_base};
    }
    Type type() const;
  };

  struct MemberIterator {
    Library const& l;
    u32 i;
    u32 struct_base;
    Member operator*() const { return {l, i, struct_base}; }
    bool operator!=(u32 n) const { return i != n; }
    void operator++() { ++i; }
  };

  struct Members {
    Library const& l;
    u32 s;
    u32 struct_base() const { return s ? l.struct_member.offsets[s - 1] : 0; }
    Member operator[](u32 i) const { return {l, s + i, struct_base()}; }
    MemberIterator begin() {
      u32 base = struct_base();
      return {l, base, base};
    }
    u32 end() const { return l.struct_member.offsets[s]; }
  };

  struct Struct {
    Library const& l;
    u32 i;
    Str name() const { return l.names[l.struct_names[i]]; }
    Members members() const { return {l, i}; }
  };

  struct StructIterator {
    Library const& l;
    u32 i;
    Struct operator*() const { return {l, i}; }
    bool operator!=(u32 n) const { return i != n; }
    void operator++() { ++i; }
  };

  struct Structs {
    Library const& l;
    StructIterator begin() const { return {l, 0}; }
    u32 end() const { return len(l.struct_names); }
  };

  struct Type {
    Library const& l;
    u32 id;
    bool is_primitive() const { return id < PrimitiveCount; }
    Primitive primitive() const {
      check(is_primitive());
      return Primitive {PrimitiveId(id)};
    }
    Struct struct_() const {
      check(!is_primitive());
      return {l, id - PrimitiveCount};
    }
    Str name() const { return is_primitive() ? primitive().name() : struct_().name(); }
  };

  Structs structs() const { return {*this}; }

  LibraryStruct type(Str name) const {
    auto index = find_if(struct_names.span(), [&](u32 n) {
      return names[n] == name;
    });
    return type(*index);
  }

  LibraryStruct type(u32 index) const {
    auto members = struct_member[index];
    return {struct_names[index], len(members), members.begin()};
  }
};

Library parse(Str schema);

void print_to_bstruct(Library const& p, Print& s);

inline auto to_bstruct(Library const& x) {
  return [&](Print& p) { print_to_bstruct(x, p); };
}

inline Library::Type Library::Member::type() const {
  return {l, library_member().type};
}
