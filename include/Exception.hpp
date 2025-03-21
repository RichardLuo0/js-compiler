#pragma once

#include <exception>
#include <iostream>
#include <string>
#include <utility>

namespace JsCompiler {
struct CompileException : std::exception {
 protected:
  const std::string message;

 public:
  explicit CompileException(std::string message)
      : message(std::move(message)) {}

  [[nodiscard]] const char* what() const noexcept override {
    return message.c_str();
  }

  void printMessage() const { std::cout << message << std::endl; };
};

struct SyntaxException : public CompileException {
 public:
  explicit SyntaxException(const std::string& message)
      : CompileException("Syntax error: " + message) {}
};

struct UnexpectedTokenException : public SyntaxException {
 public:
  explicit UnexpectedTokenException(const std::string& token)
      : SyntaxException("Unexpected token: " + token) {}
  explicit UnexpectedTokenException(const std::string_view& token)
      : SyntaxException(std::string("Unexpected token: ").append(token)) {}
  explicit UnexpectedTokenException(char ch)
      : UnexpectedTokenException(std::string(1, ch)) {}
};
}  // namespace JsCompiler
