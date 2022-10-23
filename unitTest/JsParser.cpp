#include "JsParser.hpp"

#include <gtest/gtest.h>

#include <memory>

#include "Expression.hpp"
#include "Lexer.parser.hpp"

namespace JsCompiler {
class ParserTest : public ::testing::Test {
 protected:
  std::stringstream stream;
  std::unique_ptr<JsParser> parser =
      JsParser::create((GeneratedParser::Lexer::create(stream)));
};

TEST_F(ParserTest, MultiLineComment) {
  stream.str("/* Hello\n world */");
  auto expression = parser->parseExpression();
  EXPECT_EQ(dynamic_cast<CommentExpression*>(expression.get())->value,
            "Hello\n world ");
}

TEST_F(ParserTest, SingleLineComment) {
  stream.str(R"(// Hello world )");
  auto expression = parser->parseExpression();
  EXPECT_EQ(dynamic_cast<CommentExpression*>(expression.get())->value,
            "Hello world ");
}

TEST_F(ParserTest, VariableStatement) {
  stream.str(R"(var a = null)");
  auto expression = parser->parseExpression();
  EXPECT_EQ(dynamic_cast<IdentifierExpression*>(expression.get())->name, "a");
}

TEST_F(ParserTest, ImportStatement) {
  stream.str(R"(import "a";)");
  auto expression = parser->parseExpression();
  EXPECT_EQ(dynamic_cast<ImportExpression*>(expression.get())->value, "\"a\"");
}
}  // namespace JsCompiler
