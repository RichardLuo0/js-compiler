#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include <memory>

#include "Parser.hpp"

namespace JsCompiler {
using namespace llvm;

class JsIRBuilder {
 private:
  const std::unique_ptr<Parser> parser;

  LLVMContext context;
  const IRBuilder<>& irBuilder;

 public:
  explicit JsIRBuilder(std::unique_ptr<Parser> parser)
      : irBuilder(std::move(IRBuilder<>(context))), parser(std::move(parser)){};

  void build() { parser->parseExpression()->codegen(); }
};
}  // namespace JsCompiler
