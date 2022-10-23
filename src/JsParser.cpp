#include "JsParser.hpp"

#include <memory>
#include <queue>
#include <stdexcept>

#include "Exception.hpp"
#include "Expression.hpp"
#include "NonTerminal.parser.hpp"
#include "Serializer.parser.hpp"
#include "Utility.hpp"

using namespace JsCompiler;
using namespace GeneratedParser;
using namespace Serializer;

extern const BinaryIType js_ebnf[];

JsParser::JsParser(std::unique_ptr<Lexer> lexer)
    : Parser(std::move(lexer),
             BinaryDeserializer::create<ArrayStream>(js_ebnf)){};

std::unique_ptr<JsCompiler::Expression> JsParser::parseExpression() {
  const auto& root = Parser::parseExpression();
  std::stack<const Node*> postOrderStack;
  std::stack<const Node*> traverseStack({&root});
  while (!traverseStack.empty()) {
    const Node& node = *traverseStack.top();
    traverseStack.pop();
    if (node.symbol.type == Symbol::End) continue;
    if (!node.children.empty() || node.symbol.type == Symbol::Terminal)
      postOrderStack.push(&node);
    for (const Node& child : node.children) {
      traverseStack.push(&child);
    }
  }

  std::list<std::string> terminalList;
  std::queue<std::unique_ptr<Expression>> expressionQueue;
  while (!postOrderStack.empty()) {
    const Node& node = *postOrderStack.top();
    switch (node.symbol.type) {
      case Symbol::End:
        break;
      case Symbol::Terminal:
        terminalList.push_back(node.value);
        break;
      case Symbol::NonTerminal:
        switch (node.symbol.getNonTerminal()) {
          case ModuleSpecifier:
            expressionQueue.push(
                std::make_unique<ImportExpression>(terminalList.back()));
            terminalList.clear();
          default:
            break;
        }
        break;
      default:
        throw std::runtime_error("Unknown symbol type");
    }
    postOrderStack.pop();
  }
  return !expressionQueue.empty() ? std::move(expressionQueue.front())
                                  : nullptr;
}
