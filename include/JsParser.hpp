#pragma once

#include <memory>
#include <queue>

#include "Expression.hpp"
#include "Parser.parser.hpp"

namespace JsCompiler {
using namespace GeneratedParser;

class JsParser : protected Parser {
 public:
  static std::unique_ptr<JsParser> create(std::unique_ptr<Lexer> lexer) {
    return std::make_unique<JsParser>(std::move(lexer));
  }

  explicit JsParser(std::unique_ptr<Lexer> lexer) : Parser(std::move(lexer)){};

  /**
   * @return {std::unique_ptr<Expression>}  : Parsed expression. Could be
   * nullptr if input is empty.
   */
  std::unique_ptr<Expression> parseExpression();
};
}  // namespace JsCompiler
