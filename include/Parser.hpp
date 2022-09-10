#pragma once

#include <vector>

#include "Expression.hpp"
#include "LLTable.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

namespace JsCompiler {
class Parser {
 protected:
  const std::unique_ptr<Lexer> lexer;

 public:
  Parser(std::unique_ptr<Lexer> lexer) : lexer(std::move(lexer)){};

  virtual std::unique_ptr<Expression> parseExpression() const
      noexcept(false) = 0;
};

class JsParser : public Parser {
 protected:
  LLTableGenerator::LLTable table;

 public:
  JsParser(std::unique_ptr<Lexer> lexer) : Parser(std::move(lexer)){};

  static inline std::unique_ptr<JsParser> create(std::unique_ptr<Lexer> lexer) {
    return std::make_unique<JsParser>(std::move(lexer));
  }

  std::unique_ptr<Expression> parseExpression() const noexcept(false);
};
}  // namespace JsCompiler
