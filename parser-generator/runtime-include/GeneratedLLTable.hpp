#pragma once

#include "CommonLLTable.hpp"
#include "GeneratedLexer.hpp"

namespace GeneratedParser {
class GeneratedLLTable : public LLTable<std::string, TokenType> {
 public:
  GeneratedLLTable();
};
}  // namespace GeneratedParser
