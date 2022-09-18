#pragma once

#include <list>
#include <stdexcept>
#include <string>
#include <utility>

#include "GeneratedLLTable.hpp"
#include "GeneratedLexer.hpp"

namespace GeneratedParser {
using S = GeneratedLLTable::Symbol;

class Parser {
 protected:
  struct SymbolNode {
    const S symbol;
    std::string value;

    SymbolNode* previousNode;
    std::list<SymbolNode> children;

    SymbolNode(S symbol, SymbolNode* previousNode)
        : symbol(std::move(symbol)), previousNode(previousNode) {}
    explicit SymbolNode(const S& symbol) : SymbolNode(symbol, nullptr) {}

    bool operator==(const SymbolNode& another) const {
      return symbol == another.symbol;
    }

    bool operator!=(const SymbolNode& another) const {
      return symbol != another.symbol;
    }
  };

  const std::unique_ptr<Lexer> lexer;

  const GeneratedLLTable table;

 public:
  explicit Parser(std::unique_ptr<Lexer> lexer) : lexer(std::move(lexer)){};

  [[nodiscard]] virtual inline bool isInputEnd(const Token& token) const {
    return token.type == eof;
  }

  SymbolNode parseExpression() const noexcept(false) {
    // Construct tree
    std::list<SymbolNode*> epsilonNodeList;
    SymbolNode root{table.getStart()};
    SymbolNode end{GeneratedLLTable::end};
    std::stack<SymbolNode*> stack({&end, &root});
    lexer->readNextToken();
    while (!stack.empty()) {
      const Token& currentToken = lexer->getCurrentToken();
      const S& symbol = isInputEnd(currentToken) ? GeneratedLLTable::end
                                                 : S{currentToken.type};
      SymbolNode& topSymbolNode = *stack.top();
      if (topSymbolNode.symbol == symbol) {
        topSymbolNode.value = currentToken.value;
        if (!isInputEnd(currentToken)) lexer->readNextToken();
        stack.pop();
        continue;
      }
      const std::list<S>& children =
          table.predict(topSymbolNode.symbol, symbol);
      stack.pop();
      if (children.front().type != S::End) {
        for (const auto& symbol : children) {
          topSymbolNode.children.emplace_back(symbol, &topSymbolNode);
        }
        for (auto it = topSymbolNode.children.rbegin();
             it != topSymbolNode.children.rend(); it++) {
          stack.push(&*it);
        }
      } else
        epsilonNodeList.push_back(&topSymbolNode);
    }
    if (!isInputEnd(lexer->getCurrentToken()))
      throw std::runtime_error("Extra token");

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
