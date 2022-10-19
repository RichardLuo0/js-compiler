#pragma once

#include <algorithm>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "LLTableBase.parser.hpp"

namespace ParserGenerator {
/*
 * The std::hash<NonTerminalType> and std::hash<TerminalType> specialization
 * must be implemented
 */
template <typename NonTerminalType, typename TerminalType>
class LLTable : public GeneratedParser::LLTable<NonTerminalType, TerminalType> {
  using LLTableBase = GeneratedParser::LLTable<NonTerminalType, TerminalType>;

 public:
  struct Production;
  using Symbol = typename LLTableBase::Symbol;
  using SymbolSet = std::unordered_set<Symbol, typename Symbol::Hash>;
  using PreviousSet = std::unordered_set<const Production*>;

  struct Production {
    NonTerminalType left;
    std::list<Symbol> right;

    Production(NonTerminalType left, std::list<Symbol> right)
        : left(std::move(left)), right(std::move(right)) {}
    // Only used for in-place construction
    explicit Production(NonTerminalType left) : left(std::move(left)) {}

    inline bool isEnd() {
      return right.size() == 1 && right.front() == LLTableBase::END;
    }

    constexpr bool operator==(const Production& another) const {
      return toString() == another.toString();
    }

    constexpr bool operator!=(const Production& another) const {
      return !((*this) == another);
    }

    [[nodiscard]] std::string toString() const {
      std::string str;
      str += left + ", ";
      for (auto& symbol : right) {
        switch (symbol.type) {
          case Symbol::Terminal:
            str += "\"" + std::to_string(symbol.getTerminal()) + "\"";
            break;
          case Symbol::NonTerminal:
            str += symbol.getNonTerminal();
            break;
          case Symbol::End:
            str += "\"end\"";
          default:
            break;
        }
        str += " ";
      }
      return str;
    }

    struct Hash {
      size_t operator()(const Production& production) const {
        return std::hash<std::string>()(production.toString());
      }
    };
  };

 protected:
  void debugPrint(const std::list<Production>& grammar) {
    static size_t i = 0;
    std::ofstream f(".log/log" + std::to_string(i++) + ".txt");
    for (auto& production : grammar) {
      f << production.toString() << std::endl;
    }
  }

  struct Node;
  struct Edge {
    Production* production;
    Node* to;
  };

  struct Node {
    std::list<Edge> edges;
    Symbol symbol = LLTableBase::END;
  };

  auto transformToLLGrammar(
      std::list<Production>& grammar,
      const std::function<NonTerminalType(NonTerminalType)>&
          createSubNonTerminal) {
    // Fixed point iteration
    bool isChanged = true;

    // A graph to represent the first set. The first of the pair is used to
    // store all nodes, the second of the pair is used to record terminal nodes.
    std::pair<std::unordered_map<Symbol, Node, typename Symbol::Hash>,
              std::unordered_set<Node*>>
        graphInfo;

    auto& graph = graphInfo.first;
    auto& terminalNodeSet = graphInfo.second;

    auto removeUnusedProduction = [&]() {
      std::list<Production> optimizedGrammar;
      std::unordered_set<NonTerminalType> visited{this->start.getNonTerminal()};
      std::stack<const Symbol*> traverseStack({&this->start});
      while (!traverseStack.empty()) {
        const Symbol& symbol = *traverseStack.top();
        traverseStack.pop();
        for (auto& p : grammar) {
          if (p.left == symbol.getNonTerminal() && !p.right.empty()) {
            optimizedGrammar.push_back(p);
            for (auto& symbol : p.right) {
              if (symbol.type == Symbol::NonTerminal &&
                  !visited.contains(symbol.getNonTerminal())) {
                visited.insert(symbol.getNonTerminal());
                traverseStack.push(&symbol);
              }
            }
          }
        }
      }
      grammar = optimizedGrammar;
    };

    auto createFirstSetGraph = [&]() {
      graph.clear();
      terminalNodeSet.clear();
      for (auto& p : grammar) {
        const NonTerminalType& left = p.left;
        Symbol pSymbol(left);
        if (graph.contains(pSymbol)) continue;
        std::stack<Symbol*> traverseStack({&pSymbol});
        while (!traverseStack.empty()) {
          const Symbol& symbol = *traverseStack.top();
          graph[symbol].symbol = symbol;
          traverseStack.pop();
          for (auto& p2 : grammar) {
            if (p2.left == symbol.getNonTerminal()) {
              Symbol& rightFirst = p2.right.front();
              if (rightFirst.type != Symbol::NonTerminal) {
                graph[rightFirst].symbol = rightFirst;
                terminalNodeSet.insert(&graph[rightFirst]);
              } else if (!graph.contains(rightFirst))
                traverseStack.push(&rightFirst);
              graph[rightFirst].edges.push_back({&p2, &graph[symbol]});
            }
          }
        }
      }
    };

    auto removeRightFirstEndProduction = [&]() {
      const Node* endNode = nullptr;
      std::unordered_set<const Node*> visited;
      for (Node* terminalNode : terminalNodeSet) {
        if (terminalNode->symbol == LLTableBase::END) {
          endNode = terminalNode;
          continue;
        }
        std::stack<Node*> stack({terminalNode});
        while (!stack.empty()) {
          const Node* node = stack.top();
          visited.insert(node);
          stack.pop();
          for (const Edge& edge : node->edges) {
            if (!visited.contains(edge.to)) stack.push(edge.to);
          }
        }
      }
      if (endNode != nullptr)
        for (const Edge& edge : endNode->edges) {
          if (!visited.contains(edge.to))
            for (const Edge& edge2 : edge.to->edges) {
              isChanged = true;
              std::list<Symbol>& right = edge2.production->right;
              right.pop_front();
              if (right.empty()) right.push_back(LLTableBase::END);
            }
        }
    };

    /**
     * Eliminate left recursion
     * Tarjan's algorithm
     */
    auto eliminateLeftRecursion = [&]() {
      std::unordered_set<Node*> visited;
      for (Node* terminalNode : terminalNodeSet) {
        if (terminalNode->symbol == LLTableBase::END) continue;
        // DFS
        struct Context {
          Node* node;
          Node* prev = nullptr;
          Edge* edge = nullptr;

          explicit Context(Node* node) : node(node) {}
          explicit Context(Node* node, Node* prev, Edge* edge)
              : node(node), prev(prev), edge(edge) {}

          constexpr bool operator==(const Context& another) const {
            return node == another.node && prev == another.prev &&
                   edge == another.edge;
          }

          constexpr bool operator!=(const Context& another) const {
            return !((*this) == another);
          }
        };
        std::stack<Node*> pathStack;
        std::unordered_map<const Node*, Edge*> path;
        std::stack<Context> traverseStack({Context(terminalNode)});
        while (!traverseStack.empty()) {
          Node& node = *traverseStack.top().node;
          traverseStack.pop();
          size_t size = traverseStack.size();
          auto it = node.edges.begin();
          while (it != node.edges.end()) {
            Edge& edge = *it;
            if (!visited.contains(edge.to)) {
              visited.insert(edge.to);
              traverseStack.emplace(edge.to, &node, &edge);
            } else {
              path.emplace(&node, &edge);
              if (path.contains(edge.to)) {
                isChanged = true;

                Node* currentNode = edge.to;
                NonTerminalType preNonTerminal;
                do {
                  Edge& currentEdge = *path.at(currentNode);
                  Production& p = *currentEdge.production;
                  const bool isFirstEdge = currentNode == edge.to;
                  const bool isLastEdge = currentNode == &node;
                  auto left = p.left;
                  auto newRight = p.right;
                  if (isFirstEdge)
                    newRight.pop_front();
                  else {
                    newRight.pop_front();
                    newRight.emplace_front(preNonTerminal);
                  }
                  if (newRight.empty()) newRight.push_back(LLTableBase::END);
                  if (isFirstEdge && isLastEdge) {
                    p.right = newRight;
                  } else if (isFirstEdge) {
                    NonTerminalType newLeft = createSubNonTerminal(left);
                    preNonTerminal = newLeft;
                    p.left = newLeft;
                    p.right = newRight;
                  } else if (isLastEdge) {
                    grammar.emplace_back(p.left, newRight);
                  } else {
                    NonTerminalType newLeft = createSubNonTerminal(left);
                    preNonTerminal = newLeft;
                    grammar.emplace_back(newLeft, newRight);
                  }
                  currentNode = currentEdge.to;
                  if (!isLastEdge && currentNode->edges.size() > 1) {
                    NonTerminalType newLeft = createSubNonTerminal(left);
                    grammar.emplace_back(
                        newLeft,
                        std::list{edge.to->symbol, Symbol(preNonTerminal)});
                    grammar.emplace_back(newLeft, std::list{Symbol(left)});
                    Edge& nextEdge = *path.at(currentNode);
                    for (Edge& edge : currentNode->edges) {
                      if (&edge != &nextEdge) {
                        nextEdge.production->right.pop_front();
                        nextEdge.production->right.emplace_front(newLeft);
                      }
                    }
                  }
                } while (currentNode != edge.to);

                NonTerminalType newLeft =
                    createSubNonTerminal(edge.production->left);
                for (auto& p : grammar) {
                  if (p.left == edge.production->left) {
                    if (p.isEnd()) continue;
                    p.right.push_back(Symbol(newLeft));
                  }
                }
                edge.production->left = newLeft;
                grammar.push_back({newLeft, {LLTableBase::END}});
                return;
              }
              path.erase(&node);
            }
            it++;
          }
          const Context& context = traverseStack.top();
          if (traverseStack.size() > 0 && pathStack.size() > 0 &&
              size >= traverseStack.size()) {
            while (pathStack.size() > 0 && pathStack.top() != context.prev) {
              path.erase(pathStack.top());
              pathStack.pop();
            }
          } else
            pathStack.push(&node);
          if (!traverseStack.empty()) path[context.prev] = context.edge;
        }
      }
    };

    /**
     * Eliminate backtracking
     * @warning High space complexity?
     */
    auto eliminateBacktracking = [&]() {
      struct Path {
        Node* start;
        std::list<Edge*> edges;

        void extractCommonFactor(
            const Path& oldPath, std::list<Production>& grammar,
            const std::function<NonTerminalType(NonTerminalType)>&
                createSubNonTerminal) const {
          // Find the first different edge
          auto it = edges.begin();
          auto it2 = oldPath.edges.begin();
          Production* production = nullptr;
          Production* production2 = nullptr;
          Node* startNode = nullptr;
          Node* lastNextNode = start;
          do {
            startNode = lastNextNode;
            if (it == edges.end() || it2 == oldPath.edges.end())
              throw std::runtime_error("No common factor is found");
            lastNextNode = (*it)->to;
            production = (*it++)->production;
            production2 = (*it2++)->production;
          } while (production == production2);

          // Share the last production
          const NonTerminalType& left = edges.back()->production->left;
          NonTerminalType newLeft = createSubNonTerminal(left);
          grammar.push_back({left, {startNode->symbol, Symbol(newLeft)}});

          extractFront(--it, *startNode, newLeft, grammar,
                       createSubNonTerminal);
          oldPath.extractFront(--it2, *startNode, newLeft, grammar,
                               createSubNonTerminal);
        }

        void extractFront(
            typename std::list<Edge*>::const_iterator& extractStart,
            const Node& extractStartNode,
            const NonTerminalType& commonNewNonTerminal,
            std::list<Production>& grammar,
            const std::function<NonTerminalType(NonTerminalType)>&
                createSubNonTerminal) const {
          auto it = extractStart;
          NonTerminalType preNonTerminal;
          while (it != edges.end()) {
            const Edge* edge = *it;
            const bool isFirstEdge = it == extractStart;
            const bool isLastEdge = edge == *edges.rbegin();
            Production& p = *edge->production;
            auto left = p.left;
            auto newRight = p.right;
            if (isFirstEdge)
              newRight.pop_front();
            else {
              newRight.pop_front();
              newRight.emplace_front(preNonTerminal);
            }
            if (newRight.empty()) newRight.push_back(LLTableBase::END);
            if (isFirstEdge && isLastEdge) {
              p.left = commonNewNonTerminal;
              p.right = newRight;
            } else if (isFirstEdge) {
              NonTerminalType newLeft = createSubNonTerminal(left);
              preNonTerminal = newLeft;
              p.left = newLeft;
              p.right = newRight;
            } else if (isLastEdge) {
              grammar.emplace_back(commonNewNonTerminal, newRight);
            } else {
              NonTerminalType newLeft = createSubNonTerminal(left);
              preNonTerminal = newLeft;
              grammar.emplace_back(newLeft, newRight);
            }
            it++;
            if (!isLastEdge && edge->to->edges.size() > 1) {
              NonTerminalType newLeft = createSubNonTerminal(left);
              grammar.emplace_back(newLeft, std::list{extractStartNode.symbol,
                                                      Symbol(preNonTerminal)});
              grammar.emplace_back(newLeft, std::list{Symbol(left)});
              for (Edge& edge : edge->to->edges) {
                if (&edge != *it) {
                  edge.production->right.pop_front();
                  edge.production->right.emplace_front(newLeft);
                }
              }
            }
          }
        }
      };

      for (Node* terminalNode : terminalNodeSet) {
        if (terminalNode->symbol == LLTableBase::END) continue;
        // DFS
        std::unordered_map<Symbol*, Path> pathMap;
        std::unordered_set<Node*> visitedAfterTerminal;
        std::stack<Node*> traverseStack({terminalNode});
        while (!traverseStack.empty()) {
          Node& node = *traverseStack.top();
          traverseStack.pop();
          for (Edge& edge : node.edges) {
            Path& currentNodePath = pathMap[&node.symbol];
            currentNodePath.start = terminalNode;
            // The previous visited path
            Path& nextNodePath = pathMap[&edge.to->symbol];
            // The new path
            Path newNextNodePath = currentNodePath;
            newNextNodePath.edges.push_back(&edge);
            if (visitedAfterTerminal.contains(edge.to)) {
              isChanged = true;
              newNextNodePath.extractCommonFactor(nextNodePath, grammar,
                                                  createSubNonTerminal);
              return;
            }
            visitedAfterTerminal.insert(edge.to);
            traverseStack.push(edge.to);
            nextNodePath = newNextNodePath;
          }
        }
      }
    };

    while (isChanged) {
      isChanged = false;
      removeUnusedProduction();
      createFirstSetGraph();
      removeRightFirstEndProduction();
      if (isChanged) continue;
      eliminateLeftRecursion();
      if (isChanged) continue;
      eliminateBacktracking();
    }

    return graphInfo;
  }

  void createFirstSet(const std::unordered_set<Node*>& terminalNodeSet) {
    const Node* endNode = nullptr;
    for (Node* terminalNode : terminalNodeSet) {
      // DFS
      const Symbol& terminalSymbol = terminalNode->symbol;
      if (terminalSymbol == LLTableBase::END) {
        endNode = terminalNode;
        continue;
      }
      Edge dummyEdge{nullptr, terminalNode};
      std::stack<Edge*> traverseStack({&dummyEdge});
      while (!traverseStack.empty()) {
        Edge& edge = *traverseStack.top();
        Node& node = *edge.to;
        traverseStack.pop();
        if (node.symbol.type == Symbol::NonTerminal) {
          auto& leftMap = this->table[node.symbol.getNonTerminal()];
          if (leftMap.contains(terminalSymbol))
            throw std::runtime_error("Not a valid LL(1) grammar");
          leftMap.emplace(terminalSymbol, edge.production->right);
        }
        for (Edge& edge : node.edges) {
          traverseStack.push(&edge);
        }
      }
    }
    if (endNode != nullptr)
      for (const Edge& edge : endNode->edges) {
        this->table[edge.to->symbol.getNonTerminal()][LLTableBase::END] =
            edge.production->right;
      }
  }

  void createFollowSet(std::list<Production>& grammar) {
    // NonTerminal which can produce end
    std::unordered_set<NonTerminalType> endNonTerminalList;
    std::unordered_map<NonTerminalType, SymbolSet> followSetMap;
    std::stack<NonTerminalType> stack;
    for (Production& production : grammar)
      if (production.isEnd()) {
        stack.push(production.left);
        endNonTerminalList.insert(production.left);
      }
    while (!stack.empty()) {
      NonTerminalType work = stack.top();
      stack.pop();
      if (followSetMap.contains(work)) continue;
      SymbolSet& followSetOfWork = followSetMap[work];
      if (work == this->start.getNonTerminal()) {
        followSetOfWork.insert(LLTableBase::END);
        continue;
      }
      for (Production& production : grammar) {
        if (production.left == work) continue;
        std::list<Symbol>& right = production.right;
        for (auto it = right.begin(); it != right.end(); it++) {
          const Symbol& symbol = *it;
          if (symbol.type == Symbol::NonTerminal &&
              symbol.getNonTerminal() == work) {
            auto nextIt = std::next(it);
            if (nextIt == right.end()) {
              stack.push(production.left);
              followSetOfWork.insert(Symbol(production.left));
              continue;
            }
            const auto& nextSymbol = *nextIt;
            switch (nextSymbol.type) {
              case Symbol::NonTerminal: {
                // Insert first set of next symbol
                const NonTerminalType& nonTerminal =
                    nextSymbol.getNonTerminal();
                if (this->table.contains(nonTerminal)) {
                  const auto& firstSet =
                      std::views::keys(this->table.at(nonTerminal));
                  followSetOfWork.insert(firstSet.begin(), firstSet.end());
                }
                break;
              }
              default:
                followSetOfWork.insert(nextSymbol);
                break;
            }
          }
        }
      }
    }

    // Create follow set
    for (const auto& endNonTerminal : endNonTerminalList) {
      auto& leftMap = this->table[endNonTerminal];
      std::unordered_set<NonTerminalType> visited{endNonTerminal};
      std::stack<NonTerminalType> stack({endNonTerminal});
      while (!stack.empty()) {
        NonTerminalType topNonTerminal = stack.top();
        stack.pop();
        for (const Symbol& symbol : followSetMap.at(topNonTerminal)) {
          switch (symbol.type) {
            case Symbol::NonTerminal: {
              const NonTerminalType& symbolNonTerminal =
                  symbol.getNonTerminal();
              if (!visited.contains(symbolNonTerminal)) {
                visited.insert(symbolNonTerminal);
                stack.push(symbolNonTerminal);
              }
              break;
            }
            default:
              if (!leftMap.contains(symbol))
                leftMap.emplace(symbol, std::list{LLTableBase::END});
              break;
          }
        }
      }
    }
  }

 public:
  /**
   * LLTable constructor
   *
   * @param  start                : The start symbol of the grammar
   * @param  grammar              : The productions of the grammar
   * @param  createSubNonTerminal : A function to create a unique
   * non-terminal for LL(1) grammar transformation. For example, if there is a
   * non terminal called A, this function can return a new non-terminal called
   * A1. You need to make sure no other non-terminal has the same name.
   */
  LLTable(NonTerminalType start, std::list<Production>& grammar,
          const std::function<NonTerminalType(NonTerminalType)>&
              createSubNonTerminal)
      : LLTableBase(std::move(start)) {
    const auto& graphInfo = transformToLLGrammar(grammar, createSubNonTerminal);
    createFirstSet(graphInfo.second);
    createFollowSet(grammar);
  }

  const auto& getTable() const { return this->table; }
};
}  // namespace ParserGenerator
