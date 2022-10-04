#pragma once

#include <memory>
#include <vector>

#include "LLTable.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

namespace ParserGenerator {
struct TerminalType {
  enum Type { String, Regex, RegexExclude } type;
  std::string value;

  bool operator==(const TerminalType& another) const {
    return type == another.type && value == another.value;
  }

  bool operator!=(const TerminalType& another) const {
    return !((*this) == another);
  }
};

class BNFParser {
 protected:
  const std::unique_ptr<Lexer> lexer;

 public:
  using Table = LLTable<std::string, TerminalType>;
  using Production = Table::Production;
  using Symbol = Table::Symbol;

  explicit BNFParser(std::unique_ptr<Lexer> lexer) : lexer(std::move(lexer)){};

  static inline std::unique_ptr<BNFParser> create(
      std::unique_ptr<Lexer> lexer) {
    return std::make_unique<BNFParser>(std::move(lexer));
  }

  [[nodiscard]] std::list<Production> parse() const noexcept(false);

  [[nodiscard]] Production parseExpression() const noexcept(false);

  [[nodiscard]] std::list<Symbol> parseRight() const noexcept(false);
};
}  // namespace ParserGenerator

template <>
struct std::hash<ParserGenerator::TerminalType> {
  size_t operator()(const ParserGenerator::TerminalType& terminal) const {
    return std::hash<ParserGenerator::TerminalType::Type>{}(terminal.type) ^
           std::hash<std::string>{}(terminal.value);
  }
};
