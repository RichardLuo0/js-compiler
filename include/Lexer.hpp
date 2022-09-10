#pragma once

#include <istream>
#include <memory>
#include <string>

namespace JsCompiler {
enum TokenType {
  Eof,
  Eol,

  // Basic type
  Number,
  Reg,
  String,
  Function,

  // Variable declaration
  Var,
  Let,
  Identifier,

  // Operator
  Assign,
  Equal,
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  LeftParen,
  RightParen,

  Comma,
};

struct Token {
  TokenType type = Eof;
  std::string value;
};

class Lexer {
 protected:
  std::istream& stream;
  Token currentToken;

 public:
  Lexer(std::istream& stream) : stream(stream){};

  virtual void readNextToken() noexcept(false) = 0;

  const Token getCurrentToken() const { return currentToken; };
};

class JsLexer : public Lexer {
 public:
  JsLexer(std::istream& stream) : Lexer(stream){};

  static inline std::unique_ptr<JsLexer> create(std::istream& stream) {
    return std::make_unique<JsLexer>(stream);
  }

  void readNextToken() noexcept(false);
};
}  // namespace JsCompiler
