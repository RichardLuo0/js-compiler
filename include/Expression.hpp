#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "Lexer.hpp"

namespace JsCompiler {
class Expression {
 public:
  void virtual codegen() const = 0;
};

class NumberExpression : public Expression {
 protected:
  const double value;

 public:
  explicit NumberExpression(double value) : value(value){};

  void codegen() const override { std::cout << value << std::endl; }
};

class IdentifierExpression : public Expression {
 protected:
  const std::string name;

 public:
  explicit IdentifierExpression(std::string name) : name(std::move(name)) {}

  [[nodiscard]] std::string getName() const { return name; }

  void codegen() const override { std::cout << name << std::endl; }
};

class OperatorExpression : public Expression {
 protected:
  const TokenType operatorType = Add;

  const std::unique_ptr<Expression> left;

  const std::unique_ptr<Expression> right;

 public:
  OperatorExpression(TokenType operatorType, std::unique_ptr<Expression> left,
                     std::unique_ptr<Expression> right)
      : operatorType(operatorType),
        left(std::move(left)),
        right(std::move(right)) {}

  void codegen() const override {
    left->codegen();
    std::cout << operatorType << std::endl;
    right->codegen();
  }
};
}  // namespace JsCompiler
