#include "Parser.hpp"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <queue>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#include "Exception.hpp"
#include "Expression.hpp"
#include "LLTable.hpp"
#include "Lexer.hpp"
#include "Utils.hpp"

using namespace JsCompiler;

typedef LLTableGenerator::LLTable Table;
typedef Table::Symbol S;

struct SymbolNode {
  const S symbol;
  std::string value;

  SymbolNode* previousNode;
  std::list<SymbolNode> children;

  SymbolNode(S symbol, SymbolNode* previousNode)
      : symbol(symbol), previousNode(previousNode) {}
  SymbolNode(S symbol) : SymbolNode(symbol, nullptr) {}

  bool operator==(const SymbolNode& another) const {
    return symbol == another.symbol;
  }
};

inline bool isInputEnd(Token token) {
  return token.type == Eof || token.type == Eol;
}

std::unique_ptr<Expression> JsParser::parseExpression() const noexcept(false) {
  // Construct tree
  std::list<SymbolNode*> epsilonNodeList;
  SymbolNode root{table.getStart()};
  SymbolNode end{table.end};
  std::stack<SymbolNode*> stack({&end, &root});
  lexer->readNextToken();
  while (!stack.empty()) {
    Token currentToken = lexer->getCurrentToken();
    S symbol = isInputEnd(currentToken) ? Table::end : S{currentToken.type};
    SymbolNode& stackSymbolNode = *stack.top();
    if (stackSymbolNode.symbol == symbol) {
      stackSymbolNode.value = currentToken.value;
      if (!isInputEnd(currentToken)) lexer->readNextToken();
      stack.pop();
      continue;
    }
    std::vector<S> children;
    try {
      children = table.predict(stackSymbolNode.symbol, symbol);
    } catch (std::out_of_range e) {
      throw SyntaxException(currentToken);
    }
    stack.pop();
    if (children.front().type != S::End) {
      for (const auto& symbol : children) {
        stackSymbolNode.children.push_back({symbol, &stackSymbolNode});
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
    } while (node != nullptr && node->children.size() == 0);
  }

  // Filter unnecessary node
  std::stack<SymbolNode*> postOrderStack;
  std::stack<SymbolNode*> traverseStack({&root});
  while (!traverseStack.empty()) {
    SymbolNode& node = *traverseStack.top();
    traverseStack.pop();
    if (node.symbol.type == S::Terminal || node.symbol.type == S::End) continue;
    if (node.children.size() >= 1 ||
        node.children.begin()->symbol.type == S::Terminal)
      postOrderStack.push(&node);
    for (SymbolNode& child : node.children) {
      traverseStack.push(&child);
    }
  }

  // Build AST through post order traversal
  std::queue<std::unique_ptr<Expression> > expressionQueue;
  while (!postOrderStack.empty()) {
    SymbolNode& node = *postOrderStack.top();
    postOrderStack.pop();
    if (node.symbol.type == S::Terminal) {
      if (node.symbol.getTerminal() == Identifier) {
        expressionQueue.push(
            std::make_unique<IdentifierExpression>(node.value));
      }
      continue;
    }
    switch (Utils::hash(node.symbol.getNonTerminal().c_str())) {
      case Utils::hash("Add"): {
        std::unique_ptr<Expression> left = std::move(expressionQueue.front());
        expressionQueue.pop();
        std::unique_ptr<Expression> right = std::move(expressionQueue.front());
        expressionQueue.pop();
        expressionQueue.push(std::make_unique<OperatorExpression>(
            Add, std::move(left), std::move(right)));
        break;
      }
      case Utils::hash("Mul"): {
        std::unique_ptr<Expression> left = std::move(expressionQueue.front());
        expressionQueue.pop();
        std::unique_ptr<Expression> right = std::move(expressionQueue.front());
        expressionQueue.pop();
        expressionQueue.push(std::make_unique<OperatorExpression>(
            Mul, std::move(left), std::move(right)));
        break;
      }
      default:
        break;
    }
  }
  return std::move(expressionQueue.front());
}
