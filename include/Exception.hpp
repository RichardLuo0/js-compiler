#pragma once

#include <iostream>
#include <string>
#include <utility>

#include "GeneratedLexer.hpp"

namespace JsCompiler {
using Token = GeneratedParser::Token;
  
struct CompileException {
 public:
  [[nodiscard]] virtual std::string getMessage() const = 0;

  void printMessage() const { std::cout << getMessage() << std::endl; };
};

struct UnexpectedTokenException : public CompileException {
 private:
  const std::string token;

 public:
  explicit UnexpectedTokenException(std::string token)
      : token(std::move(token)) {}
  explicit UnexpectedTokenException(char ch)
      : UnexpectedTokenException(std::string(1, ch)) {}

  [[nodiscard]] std::string getMessage() const override;
};

struct SyntaxException : public CompileException {
  const Token token;

 public:
  explicit SyntaxException(Token token) : token(std::move(token)) {}

  [[nodiscard]] std::string getMessage() const override;
};
}  // namespace JsCompiler
