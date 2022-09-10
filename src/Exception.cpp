#include <Exception.hpp>

using namespace JsCompiler;

std::string UnexpectedTokenException::getMessage() const {
  return "Unexpected token: " + token;
}

std::string SyntaxException::getMessage() const {
  return "Syntax error: Unexpected token" + token.value;
}
