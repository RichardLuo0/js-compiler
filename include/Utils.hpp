#pragma once

#include <cctype>
#include <functional>
#include <string>

namespace JsCompiler::Utils {
static const unsigned long hashStartNumber = 5381;
static const unsigned long hashStepNumber = 5U;
constexpr unsigned long hash(const char* str) {
  unsigned long hash = hashStartNumber;
  int c = 0;
  while ((c = static_cast<unsigned char>(*str++)))
    hash = ((hash << hashStepNumber) + hash) + c; /* hash * 33 + c */
  return hash;
}
inline unsigned long hash(const std::string& str) { return hash(str.c_str()); }
}  // namespace JsCompiler::Utils
