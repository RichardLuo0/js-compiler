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
    currentChar = static_cast<char>(stream.get());
  }

  switch (currentChar) {
    case EOF:
      currentToken = {Eof, std::string(1, currentChar)};
      return;
    case '=':
      currentToken = {Definition, std::string(1, currentChar)};
      currentChar = static_cast<char>(stream.get());
      return;
    case ';':
      currentToken = {Termination, std::string(1, currentChar)};
      currentChar = static_cast<char>(stream.get());
      return;
    case '|':
      currentToken = {Alternation, std::string(1, currentChar)};
      currentChar = static_cast<char>(stream.get());
      return;
    case '"': {
      std::string value;
      currentChar = static_cast<char>(stream.get());
      while (currentChar != '"' || value.back() == '\\') {
        value += currentChar;
        currentChar = static_cast<char>(stream.get());
      };
      currentChar = static_cast<char>(stream.get());
      if (value.empty())
        currentToken = {Epsilon, value};
      else
        currentToken = {Terminal, value};
      return;
    }
    case '/': {
      std::string value;
      do {
        value += currentChar;
        currentChar = static_cast<char>(stream.get());
      } while (currentChar != '/' || value.back() == '\\');
      value += currentChar;
      currentChar = static_cast<char>(stream.get());
      if (currentChar == 'U') {
        value += currentChar;
        currentChar = static_cast<char>(stream.get());
      }
      currentToken = {Regex, value};
      return;
    }
    default:
      if (isInNonTerminal(currentChar)) {
        std::string value;
        do {
          value += currentChar;
          currentChar = static_cast<char>(stream.get());
        } while (isInNonTerminal(currentChar));
        currentToken = {NonTerminal, value};
        return;
      }
      throw std::runtime_error(std::string("Unexpected token: ") + currentChar);
  }
}
