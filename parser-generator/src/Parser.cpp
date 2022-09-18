#include "Parser.hpp"

#include <list>

#include "Lexer.hpp"

using namespace ParserGenerator;

std::list<P> BNFParser::parse() const noexcept(false) {
  std::list<P> rules;

  lexer->readNextToken();
  Token currentToken = lexer->getCurrentToken();
  while (currentToken.type != Eof) {
    rules.push_back(parseExpression());
    while (lexer->getCurrentToken().type == Alternation) {
      lexer->readNextToken();
      rules.emplace_back(rules.back().getLeft(), parseRight());
    }
    lexer->readNextToken();
    currentToken = lexer->getCurrentToken();
  }

  return rules;
}

P BNFParser::parseExpression() const noexcept(false) {
  Token left = lexer->getCurrentToken();
  lexer->readNextToken();

  if (lexer->getCurrentToken().type != Definition) {
    throw std::runtime_error("Expected '='");
  }
  lexer->readNextToken();

  return {left.value, parseRight()};
}

std::list<S> BNFParser::parseRight() const noexcept(false) {
  std::list<S> right;
  do {
    Token token = lexer->getCurrentToken();
    switch (token.type) {
      case NonTerminal:
        right.emplace_back(token.value);
        break;
      case Terminal:
        right.emplace_back<TerminalType>({TerminalType::String, token.value});
        break;
      case Regex:
        right.emplace_back<TerminalType>({TerminalType::Regex, token.value});
        break;
      case Epsilon:
        right.push_back(Table::end);
        break;
      default:
        throw std::runtime_error("Expect symbol");
    }
    lexer->readNextToken();
  } while (lexer->getCurrentToken().type != Termination &&
           lexer->getCurrentToken().type != Alternation);
  return right;
}
