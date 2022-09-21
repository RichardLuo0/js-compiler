#include "GeneratedLexer.hpp"
#include "JsIRBuilder.hpp"
#include "JsParser.hpp"

using namespace JsCompiler;

int main(int /*argc*/, const char** /*argv*/) {
  JsIRBuilder builder(
      JsParser::create((GeneratedParser::Lexer::create(std::cin))));
  builder.build();
  return 0;
}
