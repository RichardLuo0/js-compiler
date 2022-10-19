#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include <memory>

#include "JsParser.hpp"

namespace JsCompiler {
using namespace llvm;
using namespace GeneratedParser;

class JsIRBuilder {
 private:
  const std::unique_ptr<JsParser> parser;

  LLVMContext context;
  const IRBuilder<>& irBuilder;

 public:
  explicit JsIRBuilder(std::unique_ptr<JsParser> parser)
      : parser(std::move(parser)), irBuilder(std::move(IRBuilder<>(context))){};

  void build() {
    const auto& expression = parser->parseExpression();
    if (expression != nullptr) expression->codegen();
  }
};
}  // namespace JsCompiler
