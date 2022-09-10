#include "Lexer.hpp"

#include <cctype>

#include "Exception.hpp"
#include "Utils.hpp"

using namespace JsCompiler;

inline bool isInNumber(unsigned char c) { return isdigit(c) || c == '.'; }
inline bool isInIdentifier(unsigned char c) {
  return std::isalpha(c) || c == '_' || c == '$';
}

void JsLexer::readNextToken() noexcept(false) {
  static char currentChar = ' ';

  std::string value = "";

  if (currentChar == EOF) {
    currentToken = {Eof, std::string(1, currentChar)};
    return;
  }

  while (std::isspace(currentChar)) {
    currentChar = stream.get();
  }

  // EOF
  if (currentChar == EOF) {
    value = currentChar;
    currentToken = {Eof, value};
    currentChar = stream.get();
    return;
  }

  // EOL
  if (currentChar == '\n') {
    value = currentChar;
    currentToken = {Eol, value};
    currentChar = stream.get();
    return;
  }

  // Number
  if (isInNumber(currentChar)) {
    do {
      value += currentChar;
      currentChar = stream.get();
    } while (isInNumber(currentChar));
    currentToken = {Number, value};
    return;
  }

  // String
  if (currentChar == '"') {
    currentChar = stream.get();
    while (currentChar != '"') {
      value += currentChar;
      currentChar = stream.get();
    };
    currentChar = stream.get();
    currentToken = {String, value};
    return;
  }

  // Equal
  if (currentChar == '=') {
    value = currentChar;
    currentChar = stream.get();
    if (currentChar == '=') {
      value += currentChar;
      currentToken = {Equal, value};
    } else {
      currentToken = {Assign, value};
    }
    currentChar = stream.get();
    return;
  }

  // Add
  if (currentChar == '+') {
    value = currentChar;
    currentToken = {Add, value};
    currentChar = stream.get();
    return;
  }

  // Mul
  if (currentChar == '*') {
    value = currentChar;
    currentToken = {Mul, value};
    currentChar = stream.get();
    return;
  }

  // Comma
  if (currentChar == ',') {
    value = currentChar;
    currentToken = {Comma, value};
    currentChar = stream.get();
    return;
  }

  // Keywords and Identifier
  if (isInIdentifier(currentChar)) {
    TokenType type;
    do {
      value += currentChar;
      currentChar = stream.get();
    } while (isInIdentifier(currentChar));
    switch (Utils::hash(value.c_str())) {
      case Utils::hash("var"):
        type = Var;
        break;
      case Utils::hash("let"):
        type = Let;
        break;
      case Utils::hash("function"):
        type = Function;
        break;
      default:
        type = Identifier;
        break;
    }
    currentToken = {type, value};
    return;
  }

  throw UnexpectedTokenException(currentChar);
}
