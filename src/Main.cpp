#include <llvm/IR/LLVMContext.h>

#include <iostream>
#include <memory>

#include "JsIRBuilder.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

using namespace JsCompiler;

int main(int  /*argc*/, const char**  /*argv*/) {
  JsIRBuilder builder((JsParser::create((JsLexer::create(std::cin)))));
  builder.build();
  return 0;
}
