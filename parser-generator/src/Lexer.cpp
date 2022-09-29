#include "Lexer.hpp"

#include "Utility.hpp"

using namespace ParserGenerator;

inline bool isInNonTerminal(unsigned char c) {
  return std::isalpha(c) || std::isdigit(c);
}

inline void read(char& currentChar, std::istream& stream) {
  currentChar = static_cast<char>(stream.get());
}

void BNFLexer::readNextToken() noexcept(false) {
  static char currentChar = ' ';

  if (currentChar == EOF) {
    currentToken = {Eof, std::string(1, currentChar)};
    return;
  }

  while (std::isspace(currentChar) || currentChar == '\n') {
    read(currentChar, stream);
  }

  switch (currentChar) {
    case EOF:
      currentToken = {Eof, std::string(1, currentChar)};
      return;
    case '=':
      currentToken = {Definition, std::string(1, currentChar)};
      read(currentChar, stream);
      return;
    case ';':
      currentToken = {Termination, std::string(1, currentChar)};
      read(currentChar, stream);
      return;
    case '|':
      currentToken = {Alternation, std::string(1, currentChar)};
      read(currentChar, stream);
      return;
    case '"': {
      std::string value;
      read(currentChar, stream);
      while (currentChar != '"' || value.back() == '\\') {
        value += currentChar;
        read(currentChar, stream);
      };
      read(currentChar, stream);
      if (value.empty())
        currentToken = {Epsilon, value};
      else
        currentToken = {StringTerminal, value};
      return;
    }
    case '/': {
      std::string value;
      do {
        value += currentChar;
        read(currentChar, stream);
      } while (currentChar != '/' || value.back() == '\\');
      value += currentChar;
      read(currentChar, stream);
      if (currentChar == 'U') {
        value += currentChar;
        read(currentChar, stream);
      }
      currentToken = {RegexTerminal, Utility::escape(value)};
      return;
    }
    case '(': {
      std::string value;
      read(currentChar, stream);
      if (currentChar != '*') throw std::runtime_error("Expecting * after (");
      read(currentChar, stream);
      while (currentChar != ')' || value.back() != '*') {
        value += currentChar;
        read(currentChar, stream);
      }
      read(currentChar, stream);
      currentToken = {Comment, value};
      return;
    }
    default:
      if (isInNonTerminal(currentChar)) {
        std::string value;
        do {
          value += currentChar;
          read(currentChar, stream);
        } while (isInNonTerminal(currentChar));
        currentToken = {NonTerminal, value};
        return;
      }
      throw std::runtime_error(std::string("Unexpected token: ") + currentChar);
  }
}
