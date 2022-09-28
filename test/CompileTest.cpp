#include "GeneratedLexer.hpp"
#include "JsIRBuilder.hpp"
#include "JsParser.hpp"

using namespace JsCompiler;

int test_CompileTest(int, char**) {
  std::stringstream s(R"(/* H */)");
  JsIRBuilder builder(JsParser::create((GeneratedParser::Lexer::create(s))));
  builder.build();
  return 0;
}
