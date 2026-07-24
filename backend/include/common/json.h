// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A tiny JSON *serialization* helper -- writing, not parsing. Kept
// deliberately minimal (the API surface here is small and controlled) to
// avoid pulling in a full JSON library dependency.

#ifndef DISTRIBUTED_SEARCH_COMMON_JSON_H_
#define DISTRIBUTED_SEARCH_COMMON_JSON_H_

#include <sstream>
#include <string>

namespace dse {
namespace common {

inline std::string JsonEscape(const std::string& s) {
  std::ostringstream out;
  for (char c : s) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out << buf;
        } else {
          out << c;
        }
    }
  }
  return out.str();
}

}  // namespace common
}  // namespace dse

#endif  // DISTRIBUTED_SEARCH_COMMON_JSON_H_
