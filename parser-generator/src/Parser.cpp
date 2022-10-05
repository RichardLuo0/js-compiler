#include "Parser.hpp"

#include <list>

#include "Lexer.hpp"

using namespace ParserGenerator;

std::list<BNFParser::Production> BNFParser::parse() const noexcept(false) {
  std::list<Production> productionList;

  lexer->readNextToken();
  const Token& token = lexer->getCurrentToken();
  while (token.type != Eof) {
    if (token.type != Comment) {
      parseExpression(productionList);
      while (lexer->getCurrentToken().type == Alternation) {
        lexer->readNextToken();
        parseRight(
            productionList.emplace_back(productionList.back().left).right);
      }
    }
    lexer->readNextToken();
  }

  return productionList;
}

void BNFParser::parseExpression(std::list<Production>& productionList) const
    noexcept(false) {
  Token left = lexer->getCurrentToken();
  lexer->readNextToken();

  if (lexer->getCurrentToken().type != Definition) {
    throw std::runtime_error("Expected '='");
  }
  lexer->readNextToken();

  parseRight(productionList.emplace_back(left.value).right);
}

void BNFParser::parseRight(std::list<Symbol>& right) const noexcept(false) {
  const Token& token = lexer->getCurrentToken();
  do {
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
      case RegexTerminalExclude:
        right.emplace_back<TerminalType>(
            {TerminalType::RegexExclude, token.value});
        break;
      case Epsilon:
        right.push_back(Table::end);
        break;
      default:
        throw std::runtime_error("Expect symbol");
    }
    lexer->readNextToken();
  } while (token.type != Termination && token.type != Alternation);
}
