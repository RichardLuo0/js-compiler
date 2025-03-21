#pragma once

#include <string_view>

namespace JsCompiler::Utility {
// Compile-time string hash
static constexpr unsigned long hashStartNumber = 5381;
static constexpr unsigned long hashStepNumber = 5U;
constexpr unsigned long hash(std::string_view str) {
  unsigned long hash = hashStartNumber;
  for (const auto& ch : str) {
    hash = ((hash << hashStepNumber) + hash) + ch; /* hash * 33 + c */
  }
  return hash;
}
}  // namespace JsCompiler::Utility
