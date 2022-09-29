#pragma once

#include <string>

namespace ParserGenerator::Utility {
inline std::string escape(const std::string& str) {
  std::string str2;
  for (const auto& ch : str) {
    if (ch == '"' || ch == '\\') str2.push_back('\\');
    str2.push_back(ch);
  }
  return str2;
}
}  // namespace ParserGenerator::Utility
