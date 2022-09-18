#include "JsParser.hpp"

#include "Utils.hpp"

using namespace JsCompiler;
using namespace GeneratedParser;

std::unique_ptr<Expression> JsParser::parseExpression() {
  const auto& root = Parser::parseExpression();
  std::stack<const SymbolNode*> postOrderStack;
  std::stack<const SymbolNode*> traverseStack({&root});
  while (!traverseStack.empty()) {
    const SymbolNode& node = *traverseStack.top();
    traverseStack.pop();
    if (node.symbol.type == S::End) continue;
    if (!node.children.empty() || node.symbol.type == S::Terminal)
      postOrderStack.push(&node);
    for (const SymbolNode& child : node.children) {
      traverseStack.push(&child);
    }
  }

  std::queue<std::unique_ptr<Expression> > expressionQueue;
  while (!postOrderStack.empty()) {
    const SymbolNode& node = *postOrderStack.top();
    postOrderStack.pop();
    if (node.symbol.type == S::Terminal) {
      if (lexer->getMatcherStringFromIndex(node.symbol.getTerminal()) ==
          "/a/U") {
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
            "+", std::move(left), std::move(right)));
        break;
      }
      case Utils::hash("Minus"): {
        std::unique_ptr<Expression> left = std::move(expressionQueue.front());
        expressionQueue.pop();
        std::unique_ptr<Expression> right = std::move(expressionQueue.front());
        expressionQueue.pop();
        expressionQueue.push(std::make_unique<OperatorExpression>(
            "-", std::move(left), std::move(right)));
        break;
      }
      default:
        break;
    }
  }
  return std::move(expressionQueue.front());
}
