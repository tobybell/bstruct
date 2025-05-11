#include "parse.hh"

void to_cpp(Library const& p, Print& s) {
  for (auto struct_: p.structs()) {
    sprint(s, "struct "_s, struct_.name(), " {\n"_s);
    for (auto member: struct_.members()) {
      auto type = member.type().name();
      auto name = member.name();
      if (member.no_array())
        sprint(s, "  "_s, type, ' ', name, ";\n"_s);
      else if (member.fixed_array())
        sprint(s, "  "_s, type, ' ', name, '[', member.length_fixed(), "];\n"_s);
      else if (member.member_array())
        sprint(s, "  "_s, type, " const* "_s, name, ";\n"_s);
      else
        unreachable;
    }
    sprint(s, "};\n\n"_s);
  }
  s.chars.pop();
}
