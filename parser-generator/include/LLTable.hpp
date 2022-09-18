#pragma once

#include <corecrt.h>

#include <algorithm>
#include <cassert>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "CommonLLTable.hpp"

namespace ParserGenerator {
/*
 * The specialization of std::hash<NonTerminalType> and std::hash<TerminalType>
 * must be defined
 */
template <typename NonTerminalType, typename TerminalType>
class LLTable : public GeneratedParser::LLTable<NonTerminalType, TerminalType> {
  using CommonLLTable = GeneratedParser::LLTable<NonTerminalType, TerminalType>;

 public:
  struct Production;
  using Symbol = typename CommonLLTable::Symbol;
  using SymbolSet = std::unordered_set<Symbol, typename Symbol::Hash>;
  using PreviousSet = std::unordered_set<Production*>;

  struct ProductionBase {
    friend class LLTable;

   protected:
    NonTerminalType left;
    std::list<Symbol> right;

   public:
    ProductionBase(NonTerminalType left, std::list<Symbol> right)
        : left(left), right(right) {}

    NonTerminalType getLeft() { return left; }

    std::list<Symbol> getRight() { return right; }
  };

  struct Production : public ProductionBase {
    friend class LLTable;

   protected:
    SymbolSet firstSet;
    PreviousSet previousSet;

   public:
    Production(NonTerminalType left, std::list<Symbol> right)
        : ProductionBase(left, right) {}
  };

 protected:
  std::list<TerminalType> terminalList;

  struct Node;
  struct Edge {
    Production* production;
    Node* to;
  };

  struct Node {
    std::list<Edge> edges;
    Symbol symbol = CommonLLTable::end;
    bool isVisited = false;
  };

  auto transformToLLGrammar(
      std::list<Production>& grammar,
      const std::function<NonTerminalType(NonTerminalType)>&
          createSubNonTerminal) {
    bool isChanged = true;
    // First is used to store all nodes, second is used to record terminal nodes
    std::pair<std::unordered_map<Symbol, Node, typename Symbol::Hash>,
              std::unordered_set<Node*>>
        graphInfo;
    while (isChanged) {
      isChanged = false;
      // Avoid access new production
      size_t grammarSize = grammar.size();

      // Construct a graph
      auto& graph = graphInfo.first;
      auto& terminalNodeSet = graphInfo.second;
      graph.clear();
      terminalNodeSet.clear();
      for (auto& p : grammar) {
        const NonTerminalType& left = p.left;
        Symbol pSymbol(left);
        if (graph.count(pSymbol)) continue;
        std::stack<Symbol*> traverseStack({&pSymbol});
        while (!traverseStack.empty()) {
          Symbol& symbol = *traverseStack.top();
          graph[symbol].symbol = symbol;
          traverseStack.pop();
          for (auto& matchP : grammar) {
            if (matchP.left == symbol.getNonTerminal()) {
              Symbol& rightFirst = matchP.right.front();
              if (rightFirst.type != Symbol::NonTerminal) {
                graph[rightFirst].symbol = rightFirst;
                terminalNodeSet.insert(&graph[rightFirst]);
              } else if (!graph.count(rightFirst))
                traverseStack.push(&rightFirst);
              graph[rightFirst].edges.push_back({&matchP, &graph[symbol]});
            }
          }
        }
      }

      // Eliminate left recursion
      // Tarjan's algorithm
      for (Node* terminalNode : terminalNodeSet) {
        // DFS
        std::stack<Node*> pathStack;
        std::unordered_map<const Node*, Edge*> path;
        std::stack<Node*> traverseStack({terminalNode});
        while (!traverseStack.empty()) {
          Node& node = *traverseStack.top();
          node.isVisited = true;
          traverseStack.pop();
          size_t size = traverseStack.size();
          typename std::list<Edge>::iterator it = node.edges.begin();
          while (it != node.edges.end()) {
            Edge& edge = *it;
            if (!edge.to->isVisited) {
              traverseStack.push(edge.to);
              path[&node] = &edge;
            } else {
              if (path.count(edge.to)) {
                isChanged = true;
                Production& p = *edge.production;
                NonTerminalType newLeft = createSubNonTerminal(p.left);

                // From edge.to to node
                {
                  NonTerminalType toNonTerminal =
                      edge.to->symbol.getNonTerminal();
                  const NonTerminalType* preCopiedNonTerminal = nullptr;
                  Node* currentNode = edge.to;
                  Production& firstP = *path[currentNode]->production;
                  if (edge.to->edges.size() > 1) {
                    NonTerminalType left = node.symbol.getNonTerminal();
                    NonTerminalType newLeft2 = createSubNonTerminal(left);
                    grammar.push_back(
                        {left, {Symbol(toNonTerminal), Symbol(newLeft2)}});
                    firstP.left = newLeft2;
                    preCopiedNonTerminal = &newLeft2;
                  }
                  firstP.right.pop_front();
                  // If no right left
                  if (firstP.right.size() <= 0)
                    firstP.right.push_back(CommonLLTable::end);
                  currentNode = path[currentNode]->to;
                  while (currentNode != edge.to) {
                    if (preCopiedNonTerminal != nullptr) {
                      if (currentNode->edges.size() > 1) {
                        NonTerminalType left = node.symbol.getNonTerminal();
                        NonTerminalType newLeft = createSubNonTerminal(left);
                        auto newRight = path[currentNode]->production->right;
                        newRight.pop_front();
                        newRight.push_front(Symbol(*preCopiedNonTerminal));
                        grammar.push_back({newLeft, newRight});
                        preCopiedNonTerminal = &newLeft;
                      } else {
                        auto& right = path[currentNode]->production->right;
                        right.pop_front();
                        right.push_front(Symbol(*preCopiedNonTerminal));
                        preCopiedNonTerminal = nullptr;
                      }
                    } else if (currentNode->edges.size() > 1) {
                      NonTerminalType left = node.symbol.getNonTerminal();
                      NonTerminalType newLeft = createSubNonTerminal(left);
                      auto newRight = path[currentNode]->production->right;
                      newRight.pop_front();
                      newRight.push_front(Symbol(*preCopiedNonTerminal));
                      grammar.push_back({newLeft, newRight});
                      preCopiedNonTerminal = &newLeft;
                    }
                    currentNode = path[currentNode]->to;
                  }
                }

                // Node to edge.to
                p.left = newLeft;
                grammar.push_back({newLeft, {CommonLLTable::end}});
                size_t i = 0;
                for (auto it = grammar.begin(); i < grammarSize; i++, it++) {
                  Production& matchP = *it;
                  if (matchP.left == p.left) {
                    matchP.right.push_back(Symbol(newLeft));
                  } else if (matchP.left == node.symbol.getNonTerminal()) {
                    auto newRight = matchP.right;
                    newRight.push_back(Symbol(newLeft));
                    grammar.push_back({p.left, newRight});
                  }
                }
                // Remove from graph
                it = node.edges.erase(it);
              }
            }
            it++;
          }
          if (traverseStack.size() > 0 && pathStack.size() > 0) {
            if (size == traverseStack.size()) {
              while (pathStack.size() > 0 &&
                     pathStack.top() != traverseStack.top()) {
                if (path.count(pathStack.top())) path.erase(pathStack.top());
                pathStack.pop();
              }
            } else
              pathStack.push(&node);
          }
        }
      }

      /**
       * Eliminate backtracking
       * @warning High space complexity?
       */
      struct Path {
        Node* start;
        std::list<Edge*> edges;

        bool isExtracted = false;

        void extractCommonFactor(
            Path& oldPath, std::list<Production>& grammar,
            const std::function<NonTerminalType(NonTerminalType)>&
                createSubNonTerminal) {
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
              throw std::runtime_error("No common factor found");
            lastNextNode = (*it)->to;
            production = (*it++)->production;
            production2 = (*it2++)->production;
          } while (production == production2);

          // Share the last production
          Production& lastProduction = *edges.back()->production;
          const NonTerminalType& left = lastProduction.left;
          NonTerminalType newLeft = createSubNonTerminal(left);
          lastProduction.right = {startNode->symbol, Symbol(newLeft)};

          extractFront(--it, *startNode, newLeft, grammar,
                       createSubNonTerminal);
          if (!oldPath.isExtracted)
            oldPath.extractFront(--it2, *startNode, newLeft, grammar,
                                 createSubNonTerminal);
        }

        void extractFront(typename std::list<Edge*>::iterator& extractStart,
                          const Node& extractStartNode,
                          const NonTerminalType& lastNewNonTerminal,
                          std::list<Production>& grammar,
                          const std::function<NonTerminalType(NonTerminalType)>&
                              createSubNonTerminal) {
          isExtracted = true;
          auto it = extractStart;
          NonTerminalType* preNewNonTerminal = nullptr;
          while (it != edges.end()) {
            Edge* edge = *it;
            const bool isFirstEdge = it == extractStart;
            const bool isLastEdge = edge == *edges.rbegin();
            const bool isCreateNewP = isLastEdge ||
                                      edge->to->edges.size() > 1 ||
                                      preNewNonTerminal != nullptr;
            auto newRight = edge->production->right;
            if (isFirstEdge)
              newRight.pop_front();
            else if (preNewNonTerminal != nullptr) {
              newRight.pop_front();
              newRight.push_front(Symbol(*preNewNonTerminal));
            }
            if (newRight.empty()) newRight.push_back(CommonLLTable::end);
            if (isCreateNewP) {
              NonTerminalType newLeft =
                  isLastEdge ? lastNewNonTerminal
                             : createSubNonTerminal(edge->production->left);
              preNewNonTerminal = &newLeft;
              grammar.push_back({newLeft, newRight});
              if (!isLastEdge)
                edge->production->right = {extractStartNode.symbol,
                                           Symbol(newLeft)};
            } else
              edge->production->right = newRight;
            it++;
          }
        }
      };

      std::unordered_set<Node*> visited;
      for (Node* terminalNode : terminalNodeSet) {
        // DFS
        std::unordered_map<Symbol*, Path> pathMap;
        std::unordered_set<Node*> visitedAfterTerminal;
        std::stack<Node*> traverseStack({terminalNode});
        while (!traverseStack.empty()) {
          Node& node = *traverseStack.top();
          traverseStack.pop();
          if (!visited.count(&node))
            for (Edge& edge : node.edges) {
              Path& currentNodePath = pathMap[&node.symbol];
              currentNodePath.start = terminalNode;
              // The previous visited path
              Path& nextNodePath = pathMap[&edge.to->symbol];
              // The new path
              Path newNextNodePath = currentNodePath;
              newNextNodePath.edges.push_back(&edge);
              if (visitedAfterTerminal.count(edge.to)) {
                isChanged = true;
                newNextNodePath.extractCommonFactor(nextNodePath, grammar,
                                                    createSubNonTerminal);
              } else
                traverseStack.push(edge.to);
              nextNodePath = newNextNodePath;
            }
          visitedAfterTerminal.insert(&node);
          visited.insert(&node);
        }
      }
    }

    for (auto* node : graphInfo.second) {
      terminalList.push_back(node->symbol.getTerminal());
    }
    return graphInfo;
  }

  void createFirstSet(const std::unordered_set<Node*>& terminalNodeSet) {
    for (Node* terminalNode : terminalNodeSet) {
      // DFS
      Symbol& terminalSymbol = terminalNode->symbol;
      if (terminalSymbol == CommonLLTable::end) continue;
      Edge dummyEdge{nullptr, terminalNode};
      std::stack<Edge*> traverseStack({&dummyEdge});
      while (!traverseStack.empty()) {
        Edge& edge = *traverseStack.top();
        Node& node = *edge.to;
        traverseStack.pop();
        if (node.symbol.type == Symbol::NonTerminal) {
          auto key =
              std::make_pair(node.symbol.getNonTerminal(), terminalSymbol);
          if (this->table.count(key))
            throw std::runtime_error("Not valid LL(1) grammar");
          this->table.emplace(key, edge.production->right);
          edge.production->firstSet.insert(terminalSymbol);
        }
        for (Edge& edge : node.edges) {
          traverseStack.push(&edge);
        }
      }
    }
  }

  void createFollowSet(std::list<Production>& grammar) {
    // Construct a tree
    std::list<Production*>&& workList = {};
    std::list<Production*>&& nextWorkList = {};
    std::unordered_map<Production*, SymbolSet> leafNodeMap;
    for (Production& production : grammar) {
      if (production.right.front() == CommonLLTable::end)
        workList.push_back(&production);
    }
    while (!workList.empty()) {
      for (Production* workPointer : workList) {
        Production& work = *workPointer;
        if (work.left == this->start.getNonTerminal()) {
          leafNodeMap[workPointer].insert(CommonLLTable::end);
          continue;
        }
        for (Production& matchP : grammar) {
          if (matchP.left == work.left) continue;
          std::list<Symbol>& right = matchP.right;
          size_t i = 0;
          for (auto it = right.begin(); it != right.end(); it++) {
            const Symbol& symbol = *it;
            if (symbol.type == Symbol::NonTerminal &&
                symbol.getNonTerminal() == work.left) {
              if (i + 1 > right.size() - 1) {
                PreviousSet& previousSet = matchP.previousSet;
                if (previousSet.empty()) nextWorkList.push_back(&matchP);
                previousSet.insert(&work);
                continue;
              }
              const Symbol& nextSymbol = *std::next(it);
              switch (nextSymbol.type) {
                case Symbol::NonTerminal: {
                  for (Production& production2 : grammar) {
                    if (production2.left == nextSymbol.getNonTerminal()) {
                      SymbolSet& firstSet = production2.firstSet;
                      leafNodeMap[workPointer].insert(firstSet.begin(),
                                                      firstSet.end());
                    }
                  }
                  break;
                }
                case Symbol::Terminal:
                  leafNodeMap[workPointer].insert(nextSymbol);
                  break;
                case Symbol::End:
                  leafNodeMap[workPointer].insert(CommonLLTable::end);
                  break;
              }
            }
            i++;
          }
        }
      }
      workList = nextWorkList;
      nextWorkList = {};
    }
    // Create follow set
    for (const auto& [production, symbolSet] : leafNodeMap) {
      std::stack<Production*> stack({production});
      while (!stack.empty()) {
        Production* production = stack.top();
        stack.pop();
        PreviousSet& previousSet = production->previousSet;
        if (previousSet.empty()) {
          for (const Symbol& symbol : symbolSet) {
            auto key = std::make_pair(production->left, symbol);
            if (!this->table.count(key))
              this->table.emplace(key, production->right);
          }
          continue;
        }
        for (Production* parent : previousSet) {
          stack.push(parent);
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
  LLTable(NonTerminalType start, std::list<Production> grammar,
          std::function<NonTerminalType(NonTerminalType)> createSubNonTerminal)
      : CommonLLTable(start) {
    const auto& graphInfo = transformToLLGrammar(grammar, createSubNonTerminal);
    createFirstSet(graphInfo.second);
    createFollowSet(grammar);
  }

  auto getTable() const { return this->table; }

  auto getTerminalList() const { return this->terminalList; }
};
}  // namespace ParserGenerator
