#pragma once

#include <memory>
#include <vector>

#include "LLTable.hpp"
#include "Lexer.hpp"

namespace ParserGenerator {
struct TerminalType {
  enum { CCode, String } type;
  std::string value;

  bool operator==(const TerminalType& another) const {
    return type == another.type && value == another.value;
  }
};

using Table = LLTable<std::string, TerminalType>;
using P = Table::Production;
using S = Table::Symbol;

class BNFParser {
 protected:
  const std::unique_ptr<Lexer> lexer;

 public:
  explicit BNFParser(std::unique_ptr<Lexer> lexer) : lexer(std::move(lexer)){};

  static inline std::unique_ptr<BNFParser> create(
      std::unique_ptr<Lexer> lexer) {
    return std::make_unique<BNFParser>(std::move(lexer));
  }

  [[nodiscard]] std::list<P> parse() const noexcept(false);

  [[nodiscard]] P parseExpression() const noexcept(false);

  [[nodiscard]] std::list<S> parseRight() const noexcept(false);
};
}  // namespace ParserGenerator

template <>
struct std::hash<ParserGenerator::TerminalType> {
  size_t operator()(const ParserGenerator::TerminalType& terminal) const {
    return std::hash<std::string>{}(terminal.value);
  }
};
