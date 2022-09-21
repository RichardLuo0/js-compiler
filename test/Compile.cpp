#include "GeneratedLexer.hpp"
#include "JsIRBuilder.hpp"
#include "JsParser.hpp"

using namespace JsCompiler;

int test_Compile(int, char**) {
  std::stringstream s(R"(// Hello world)");
  JsIRBuilder builder(JsParser::create((GeneratedParser::Lexer::create(s))));
  builder.build();
  return 0;
}
