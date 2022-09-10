#include "Lexer.hpp"

#include <stdexcept>
#include <string>

using namespace ParserGenerator;

inline bool isInNonTerminal(unsigned char c) {
  return std::isalpha(c) || std::isdigit(c);
}

void BNFLexer::readNextToken() noexcept(false) {
  static char currentChar = ' ';

  if (currentChar == EOF) {
    currentToken = {Eof, std::string(1, currentChar)};
    return;
  }

  while (std::isspace(currentChar) || currentChar == '\n') {
    currentChar = stream.get();
  }

  switch (currentChar) {
    case '(':
      currentChar = stream.get();
      if (currentChar == '*') {
        do {
          currentChar = stream.get();
        } while (currentChar != '*' || stream.get() != ')');
        currentChar = stream.get();
        return readNextToken();
      } else
        throw std::runtime_error("Expected '*'");
    case EOF:
      currentToken = {Eof, std::string(1, currentChar)};
      return;
    case '=':
      currentToken = {Definition, std::string(1, currentChar)};
      currentChar = stream.get();
      return;
    case ';':
      currentToken = {Termination, std::string(1, currentChar)};
      currentChar = stream.get();
      return;
    case '|':
      currentToken = {Alternation, std::string(1, currentChar)};
      currentChar = stream.get();
      return;
    case '"': {
      std::string value;
      currentChar = stream.get();
      while (currentChar != '"') {
        value += currentChar;
        currentChar = stream.get();
      };
      currentChar = stream.get();
      if (value.empty())
        currentToken = {Epsilon, value};
      else
        currentToken = {Terminal, value};
      return;
    }
    case '$': {
      std::string value = "";
      currentChar = stream.get();
      if (currentChar == '"') {
        currentChar = stream.get();
        while (currentChar != '"') {
          value += currentChar;
          currentChar = stream.get();
        };
        currentChar = stream.get();
        currentToken = {CCode, value};
        return;
      }
    }
    default:
      if (isInNonTerminal(currentChar)) {
        std::string value;
        do {
          value += currentChar;
          currentChar = stream.get();
        } while (isInNonTerminal(currentChar));
        currentToken = {NonTerminal, value};
        return;
      }
      throw std::runtime_error(std::string("Unexpected token: ") + currentChar);
  }
}
