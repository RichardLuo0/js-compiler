#include <gtest/gtest.h>

#include "GeneratedLexer.parser.hpp"
#include "JsParser.hpp"

namespace JsCompiler {
TEST(Compile, MultiLineComment) {
  std::stringstream s(R"(/* Hello world */)");
  auto expression =
      JsParser::create((GeneratedParser::Lexer::create(s)))->parseExpression();
  EXPECT_EQ(dynamic_cast<CommentExpression*>(expression.get())->value, "Hello world ");
}

TEST(Compile, SingleLineComment) {
  std::stringstream s(R"(// Hello world )");
  auto expression =
      JsParser::create((GeneratedParser::Lexer::create(s)))->parseExpression();
  EXPECT_EQ(dynamic_cast<CommentExpression*>(expression.get())->value, "Hello world ");
}
}  // namespace JsCompiler
