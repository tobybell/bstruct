#pragma once

struct Stream {
  List<char> bytes;
  friend u32 len(Stream const& s) { return len(s.bytes); }
  char* reserve(u32 n) {
    bytes.expand(bytes.size + n);
    return bytes.begin() + bytes.size;
  }
  void grow(u32 n) { bytes.size += n; }
  void grow(char* end) { bytes.size = u32(end - bytes.begin()); }
};
