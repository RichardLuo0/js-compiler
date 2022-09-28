#pragma once

#include <ranges>
#include <stdexcept>

#include "GeneratedLLTable.parser.hpp"
#include "GeneratedLexer.parser.hpp"

namespace GeneratedParser {
using Symbol = GeneratedLLTable::Symbol;

class Parser {
 protected:
  const std::unique_ptr<Lexer> lexer;

  const GeneratedLLTable table;

  struct SymbolNode {
    const Symbol symbol;
    std::string value;

    SymbolNode* previousNode;
    std::list<SymbolNode> children;

    SymbolNode(Symbol symbol, SymbolNode* previousNode)
        : symbol(std::move(symbol)), previousNode(previousNode) {}
    explicit SymbolNode(const Symbol& symbol) : SymbolNode(symbol, nullptr) {}

    bool operator==(const SymbolNode& another) const {
      return symbol == another.symbol;
    }

    bool operator!=(const SymbolNode& another) const {
      return symbol != another.symbol;
    }
  };

 public:
  explicit Parser(std::unique_ptr<Lexer> lexer) : lexer(std::move(lexer)){};

  [[nodiscard]] virtual inline bool isEof(const Token& token) const {
    return token.type == eof;
  }

  SymbolNode parseExpression() const noexcept(false) {
    // Construct tree
    std::list<SymbolNode*> epsilonNodeList;
    SymbolNode root{table.getStart()};
    SymbolNode end{GeneratedLLTable::end};
    std::stack<SymbolNode*> stack({&end, &root});
    lexer->readNextTokenExpect(
        table.getCandidate(root.symbol.getNonTerminal()));
    while (!stack.empty()) {
      SymbolNode& topSymbolNode = *stack.top();
      const Token& currentToken = lexer->getCurrentToken();
      const Symbol& symbol = isEof(currentToken) ? GeneratedLLTable::end
                                                 : Symbol(currentToken.type);

      if (topSymbolNode.symbol.type != Symbol::NonTerminal) {
        if (topSymbolNode.symbol == symbol) {
          topSymbolNode.value = currentToken.value;
          stack.pop();
          if (!isEof(currentToken)) {
            if (!stack.empty()) {
              const auto& topSymbol = stack.top()->symbol;
              if (topSymbol.type == Symbol::NonTerminal)
                lexer->readNextTokenExpect(
                    table.getCandidate(topSymbol.getNonTerminal()));
              else if (topSymbol.type == Symbol::Terminal)
                lexer->readNextTokenExpect(std::list{topSymbol.getTerminal()});
              else
                lexer->readNextTokenExpectEof();
            } else
              throw std::runtime_error("Extra token");
          }
          continue;
        }
        throw std::runtime_error("Unexpected token");
      }

      const std::list<Symbol>& children =
          table.predict(topSymbolNode.symbol, symbol);
      stack.pop();
      if (children.front().type != Symbol::End) {
        for (const auto& symbol : children) {
          topSymbolNode.children.emplace_back(symbol, &topSymbolNode);
        }
        for (auto& it : std::ranges::reverse_view(topSymbolNode.children)) {
          stack.push(&it);
        }
      } else
        epsilonNodeList.push_back(&topSymbolNode);
    }

    // Remove epsilon node
    for (SymbolNode* node : epsilonNodeList) {
      do {
        SymbolNode* previousNode = node->previousNode;
        previousNode->children.remove(*node);
        node = previousNode;
      } while (node != nullptr && node->children.empty());
    }

    return root;
  };
};
}  // namespace GeneratedParser
