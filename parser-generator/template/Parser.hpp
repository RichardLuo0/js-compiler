#pragma once

#include <list>
#include <stdexcept>
#include <utility>

#include "LLTable.hpp"
#include "Lexer.hpp"

namespace GeneratedParser {
using S = LLTable::Symbol;

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
};

class Parser {
 protected:
  const std::unique_ptr<Lexer> lexer;

  LLTable table;

 public:
  explicit Parser(std::unique_ptr<Lexer> lexer) : lexer(std::move(lexer)){};

  static inline bool isInputEnd(const Token& token) {
    return token.type == eof || token.type == eol;
  }

  std::stack<SymbolNode> parseExpression() const noexcept(false) {
    // Construct tree
    std::list<SymbolNode*> epsilonNodeList;
    SymbolNode root{table.getStart()};
    SymbolNode end{LLTable::end};
    std::stack<SymbolNode*> stack({&end, &root});
    lexer->readNextToken();
    while (!stack.empty()) {
      Token currentToken = lexer->getCurrentToken();
      S symbol = isInputEnd(currentToken) ? LLTable::end : S{currentToken.type};
      SymbolNode& stackSymbolNode = *stack.top();
      if (stackSymbolNode.symbol == symbol) {
        stackSymbolNode.value = currentToken.value;
        if (!isInputEnd(currentToken)) lexer->readNextToken();
        stack.pop();
        continue;
      }
      std::vector<S> children;
      children = table.predict(stackSymbolNode.symbol, symbol);
      stack.pop();
      if (children.front().type != S::End) {
        for (const auto& symbol : children) {
          stackSymbolNode.children.emplace_back(symbol, &stackSymbolNode);
        }
        std::list<SymbolNode>& nodeChildren = stackSymbolNode.children;
        for (auto it = nodeChildren.rbegin(); it != nodeChildren.rend(); it++) {
          stack.push(&*it);
        }
      } else
        epsilonNodeList.push_back(&stackSymbolNode);
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

    // Filter unnecessary node
    std::stack<SymbolNode> postOrderStack;
    std::stack<SymbolNode*> traverseStack({&root});
    while (!traverseStack.empty()) {
      SymbolNode& node = *traverseStack.top();
      traverseStack.pop();
      if (node.symbol.type == S::Terminal || node.symbol.type == S::End)
        continue;
      if (!node.children.empty() ||
          node.children.begin()->symbol.type == S::Terminal)
        postOrderStack.push(node);
      for (SymbolNode& child : node.children) {
        traverseStack.push(&child);
      }
    }

    return postOrderStack;
  };
};
}  // namespace GeneratedParser