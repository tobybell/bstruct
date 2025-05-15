#include "parse.hh"

struct ContiguousChunk {
  u32 member;
  u32 size;
  u32 len_member = 0;
  bool direct() const { return !len_member; }
};

void to_cpp(Library const& p, Print& s) {
  u32 total_size = 0;
  u32 len_member = 0;
  u32 len_member_size;

  List<ContiguousChunk> chunks;

  sprint(s, "#include <string.h>\n"_s);
  sprint(s, "extern \"C\" [[noreturn]] void abort();\n"_s);
  for (auto struct_: p.structs()) {
    sprint(s, "struct "_s, struct_.name(), " {\n"_s);
    auto members = struct_.members();
    for (auto member: members) {
      auto type = member.type();
      auto type_name = type.name();
      auto name = member.name();
      if (member.no_array()) {
        sprint(s, "  "_s, type_name, ' ', name, ";\n"_s);
        u32 member_size = type.primitive().size();
        total_size += member_size;
        if (chunks && chunks.last().direct())
          chunks.last().size += member_size;
        else
          chunks.push({member.index(), member_size});
      } else if (member.fixed_array()) {
        sprint(
            s, "  "_s, type_name, ' ', name, '[', member.length_fixed(),
            "];\n"_s);
        u32 member_size = type.primitive().size() * member.length_fixed();
        total_size += member_size;
        if (chunks && chunks.last().direct())
          chunks.last().size += member_size;
        else
          chunks.push({member.index(), member_size});
      } else if (member.member_array()) {
        sprint(s, "  "_s, type_name, " const* "_s, name, ";\n"_s);
        u32 len_member_id = member.length_member().index();
        len_member = len_member_id + 1;
        len_member_size = type.primitive().size();
        chunks.push({member.index(), len_member_size, len_member_id + 1});
      } else
        unreachable;
    }
    sprint(
        s, "  unsigned serialized_size() const {\n    return "_s, total_size);
    if (len_member) {
      sprint(s, " + "_s);
      check(len_member_size);
      if (len_member_size != 1)
        sprint(s, len_member_size, " * "_s);
      sprint(s, members[len_member - 1].name());
    }
    sprint(s, ";\n  }\n"_s);
    sprint(s, "  char* serialize(char* dst) const {\n"_s);
    sprint(s, "    unsigned n {};\n"_s);
    for (auto& chunk: chunks) {
      if (chunk.direct()) {
        sprint(
            s, "    memcpy(dst, &"_s, members[chunk.member].name(), ", n = "_s,
            chunk.size);
      } else {
        sprint(
            s, "    memcpy(dst, "_s, members[chunk.member].name(), ", n = "_s);
        if (chunk.size != 1)
          sprint(s, chunk.size, " * "_s);
        sprint(s, members[chunk.len_member - 1].name());
      }
      sprint(s, "); dst += n;\n"_s);
    }
    sprint(s, "    return dst;\n  };\n};\n\n"_s);
  }
  s.chars.pop();
}
