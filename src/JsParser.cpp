#include "JsParser.hpp"

#include "Exception.hpp"
#include "Utility.hpp"

using namespace JsCompiler;
using namespace GeneratedParser;

std::unique_ptr<Expression> JsParser::parseExpression() {
  const auto& root = Parser::parseExpression();
  std::stack<const SymbolNode*> postOrderStack;
  std::stack<const SymbolNode*> traverseStack({&root});
  while (!traverseStack.empty()) {
    const SymbolNode& node = *traverseStack.top();
    traverseStack.pop();
    if (node.symbol.type == Symbol::End) continue;
    if (!node.children.empty() || node.symbol.type == Symbol::Terminal)
      postOrderStack.push(&node);
    for (const SymbolNode& child : node.children) {
      traverseStack.push(&child);
    }
  }

  std::queue<std::unique_ptr<Expression> > expressionQueue;
  while (!postOrderStack.empty()) {
    const SymbolNode& node = *postOrderStack.top();
    postOrderStack.pop();
    if (node.symbol.type == Symbol::Terminal) {
      continue;
    }
    const std::string& nonTerminal = node.symbol.getNonTerminal();
    switch (Utility::hash(nonTerminal)) {
      case Utility::hash("SingleLineCommentChars"):
      case Utility::hash("MultiLineCommentChars"):
        if (node.children.size() == 1) {
          expressionQueue.push(
              std::make_unique<CommentExpression>(node.children.front().value));
        }
        break;
      case Utility::hash("Identifier"):
        if (node.children.size() == 1) {
          expressionQueue.push(std::make_unique<IdentifierExpression>(
              node.children.front().value));
        }
        break;
      case Utility::hash("Add"): {
        if (expressionQueue.size() < 2)
          throw UnexpectedTokenException(nonTerminal);
        std::unique_ptr<Expression> left = std::move(expressionQueue.front());
        expressionQueue.pop();
        std::unique_ptr<Expression> right = std::move(expressionQueue.front());
        expressionQueue.pop();
        expressionQueue.push(std::make_unique<OperatorExpression>(
            "+", std::move(left), std::move(right)));
        break;
      }
      case Utility::hash("Minus"): {
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
  return !expressionQueue.empty() ? std::move(expressionQueue.front())
                                  : nullptr;
}
