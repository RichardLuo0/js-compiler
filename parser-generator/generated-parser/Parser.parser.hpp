#pragma once

#include <stdexcept>

#include "LLTable.parser.hpp"
#include "Lexer.parser.hpp"
#include "Serializer.parser.hpp"

namespace GeneratedParser {
using Symbol = GeneratedLLTable::Symbol;

class Parser {
 protected:
  std::unique_ptr<Lexer> lexer;

  GeneratedLLTable table;

  struct Node {
    const Symbol symbol;
    std::string value;

    Node* previousNode;
    std::list<Node> children;

    Node(const Symbol& symbol, Node* previousNode)
        : symbol(symbol), previousNode(previousNode) {}
    explicit Node(const Symbol& symbol) : Node(symbol, nullptr) {}

    bool operator==(const Node& another) const {
      return symbol == another.symbol;
    }

    bool operator!=(const Node& another) const {
      return symbol != another.symbol;
    }
  };

 public:
  explicit Parser(std::unique_ptr<Lexer> lexer,
                  Serializer::BinaryDeserializer deserializer)
      : lexer(std::move(lexer)) {
    deserializer.deserialize(this->lexer->matcherList);
    deserializer.deserialize(table.table);
  }

  [[nodiscard]] virtual inline bool isEof(const Token& token) const {
    return token.type == Eof;
  }

  Node parseExpression() noexcept(false) {
    // Construct tree
    std::list<Node*> epsilonNodeList;
    Node root{table.getStart()};
    Node end{GeneratedLLTable::END};
    std::stack<Node*> stack({&end, &root});
    const Token& currentToken = lexer->getCurrentToken();
    lexer->readNextTokenExpect(
        table.getCandidate(root.symbol.getNonTerminal()));
    while (!stack.empty()) {
      Node& currentNode = *stack.top();
      const Symbol& symbol = isEof(currentToken) ? GeneratedLLTable::END
                                                 : Symbol(currentToken.type);

      if (currentNode.symbol.type != Symbol::NonTerminal) {
        if (currentNode.symbol == symbol) {
          currentNode.value = currentToken.value;
          stack.pop();
          if (!isEof(currentToken)) {
            if (!stack.empty()) {
              const auto& currentSymbol = stack.top()->symbol;
              if (currentSymbol.type == Symbol::NonTerminal)
                lexer->readNextTokenExpect(
                    table.getCandidate(currentSymbol.getNonTerminal()));
              else if (currentSymbol.type == Symbol::Terminal)
                lexer->readNextTokenExpect(
                    std::list{currentSymbol.getTerminal()});
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
          table.predict(currentNode.symbol, symbol);
      stack.pop();
      if (children.front().type != Symbol::End) {
        for (const auto& symbol : children) {
          currentNode.children.emplace_back(symbol, &currentNode);
        }
        for (auto& it : std::ranges::reverse_view(currentNode.children)) {
          stack.push(&it);
        }
      } else
        epsilonNodeList.push_back(&currentNode);
    }

    // Remove epsilon node
    for (Node* node : epsilonNodeList) {
      do {
        Node* previousNode = node->previousNode;
        previousNode->children.remove(*node);
        node = previousNode;
      } while (node != nullptr && node->children.empty());
    }

    return root;
  };
};
}  // namespace GeneratedParser
