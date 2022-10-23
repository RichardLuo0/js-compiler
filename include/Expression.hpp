#pragma once

#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace JsCompiler {
class Expression {
 public:
  void virtual codegen() const = 0;
};

class ImportExpression : public Expression {
  FRIEND_TEST(ParserTest, ImportStatement);

 protected:
  const std::string value;

 public:
  explicit ImportExpression(std::string value) : value(std::move(value)){};

  void codegen() const override { std::cout << value << std::endl; }
};

class CommentExpression : public Expression {
  FRIEND_TEST(ParserTest, MultiLineComment);
  FRIEND_TEST(ParserTest, SingleLineComment);

 protected:
  const std::string value;

 public:
  explicit CommentExpression(std::string value) : value(std::move(value)){};

  void codegen() const override { std::cout << value << std::endl; }
};

class NumberExpression : public Expression {
 protected:
  const double value;

 public:
  explicit NumberExpression(double value) : value(value){};

  void codegen() const override { std::cout << value << std::endl; }
};

class IdentifierExpression : public Expression {
  FRIEND_TEST(ParserTest, VariableStatement);

 protected:
  const std::string name;

 public:
  explicit IdentifierExpression(std::string name) : name(std::move(name)) {}

  void codegen() const override { std::cout << name << std::endl; }
};

class OperatorExpression : public Expression {
 protected:
  const std::string operatorStr;

  const std::unique_ptr<Expression> left;
  const std::unique_ptr<Expression> right;

 public:
  OperatorExpression(std::string operatorStr, std::unique_ptr<Expression> left,
                     std::unique_ptr<Expression> right)
      : operatorStr(std::move(operatorStr)),
        left(std::move(left)),
        right(std::move(right)) {}

  void codegen() const override {
    left->codegen();
    std::cout << operatorStr << std::endl;
    right->codegen();
  }
};
}  // namespace JsCompiler
