#pragma once

#include <string>

namespace ParserGenerator::Utils {
inline std::string escapeInPlace(const std::string& str) {
  std::string str2;
  for (const auto& ch : str) {
    if (ch == '"' || ch == '\\') str2.push_back('\\');
    str2.push_back(ch);
  }
  return str2;
}
}  // namespace ParserGenerator::Utils
