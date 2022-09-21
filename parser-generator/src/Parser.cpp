#include "Parser.hpp"

#include <list>

#include "Lexer.hpp"

using namespace ParserGenerator;

std::list<BNFParser::Production> BNFParser::parse() const noexcept(false) {
  std::list<Production> productionList;

  lexer->readNextToken();
  Token currentToken = lexer->getCurrentToken();
  while (currentToken.type != Eof) {
    if (currentToken.type != Comment) {
      productionList.push_back(parseExpression());
      while (lexer->getCurrentToken().type == Alternation) {
        lexer->readNextToken();
        productionList.emplace_back(productionList.back().left, parseRight());
      }
    }
    lexer->readNextToken();
    currentToken = lexer->getCurrentToken();
  }

  return productionList;
}

BNFParser::Production BNFParser::parseExpression() const noexcept(false) {
  Token left = lexer->getCurrentToken();
  lexer->readNextToken();

  if (lexer->getCurrentToken().type != Definition) {
    throw std::runtime_error("Expected '='");
  }
  lexer->readNextToken();

  return {left.value, parseRight()};
}

std::list<BNFParser::Symbol> BNFParser::parseRight() const noexcept(false) {
  std::list<Symbol> right;
  do {
    Token token = lexer->getCurrentToken();
    switch (token.type) {
      case NonTerminal:
        right.emplace_back(token.value);
        break;
      case StringTerminal:
        right.emplace_back<TerminalType>({TerminalType::String, token.value});
        break;
      case RegexTerminal:
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
