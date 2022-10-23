#include "LLTable.hpp"

namespace ParserGenerator {
template <typename NonTerminalType, typename TerminalType>
class LLTablePasses {
  using LLTableBase =
      GeneratedParser::LLTableBase<NonTerminalType, TerminalType>;
  using LLTable = ParserGenerator::LLTable<NonTerminalType, TerminalType>;
  using Production = typename LLTable::Production;
  using Symbol = typename LLTable::Symbol;

  using Node = typename LLTable::Node;
  using Edge = typename LLTable::Edge;
  using FirstSetGraph = typename LLTable::FirstSetGraph;

  using GrammarInfo = typename LLTable::GrammarInfo;

 public:
  struct RemoveUnusedProduction : public LLTable::OptimizationPass {
    void operator()(GrammarInfo& grammarInfo) override {
      auto& [grammar, _, start] = grammarInfo;
      std::list<Production> optimizedGrammar;
      std::unordered_set<NonTerminalType> visited{start};
      const Symbol startSymbol = Symbol::createNonTerminal(start);
      std::stack<const Symbol*> traverseStack({&startSymbol});
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
    }
  };

  struct BuildFirstSetGraph
      : public LLTable::template AnalysisPass<FirstSetGraph> {
    void operator()(const GrammarInfo& grammarInfo,
                    FirstSetGraph& graph) override {
      auto& grammar = grammarInfo.grammar;
      auto& [graphMap, terminalNodeSet] = graph;
      graphMap.clear();
      terminalNodeSet.clear();
      for (auto& p : grammar) {
        const NonTerminalType& left = p.left;
        const Symbol leftSymbol = Symbol::createNonTerminal(left);
        if (graphMap.contains(leftSymbol)) continue;
        std::stack<const Symbol*> traverseStack({&leftSymbol});
        while (!traverseStack.empty()) {
          const Symbol& symbol = *traverseStack.top();
          graphMap[symbol].symbol = symbol;
          traverseStack.pop();
          for (auto& p2 : grammar) {
            if (p2.left == symbol.getNonTerminal()) {
              const Symbol& rightFirst = p2.right.front();
              if (rightFirst.type != Symbol::NonTerminal) {
                graphMap[rightFirst].symbol = rightFirst;
                terminalNodeSet.insert(&graphMap[rightFirst]);
              } else if (!graphMap.contains(rightFirst))
                traverseStack.push(&rightFirst);
              graphMap[rightFirst].edges.emplace_back(&p2, &graphMap[symbol]);
            }
          }
        }
      }
    }
  };

  class RemoveRightFirstEndProduction
      : public LLTable::template TransformPass<FirstSetGraph> {
    bool operator()(GrammarInfo&, const FirstSetGraph& graph) override {
      auto& terminalNodeSet = graph.second;
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
              std::list<Symbol>& right = edge2.production->right;
              right.pop_front();
              if (right.empty()) right.push_back(LLTableBase::END);
            }
        }
      return false;
    }
  };

  /**
   * Eliminate left recursion
   * Tarjan's algorithm
   */
  class EliminateLeftRecursion
      : public LLTable::template TransformPass<FirstSetGraph> {
    bool operator()(GrammarInfo& grammarInfo,
                    const FirstSetGraph& graph) override {
      auto& [grammar, createSubNonTerminal, _] = grammarInfo;
      auto& terminalNodeSet = graph.second;
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
                    newRight.emplace_front(Symbol::NonTerminal, preNonTerminal);
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
                        std::list{edge.to->symbol,
                                  Symbol::createNonTerminal(preNonTerminal)});
                    grammar.emplace_back(
                        newLeft, std::list{Symbol::createNonTerminal(left)});
                    Edge& nextEdge = *path.at(currentNode);
                    for (Edge& edge : currentNode->edges) {
                      if (&edge != &nextEdge) {
                        nextEdge.production->right.pop_front();
                        nextEdge.production->right.emplace_front(
                            Symbol::NonTerminal, newLeft);
                      }
                    }
                  }
                } while (currentNode != edge.to);

                NonTerminalType newLeft =
                    createSubNonTerminal(edge.production->left);
                for (auto& p : grammar) {
                  if (p.left == edge.production->left) {
                    if (p.isEnd()) continue;
                    p.right.push_back(Symbol::createNonTerminal(newLeft));
                  }
                }
                edge.production->left = newLeft;
                grammar.push_back({newLeft, {LLTableBase::END}});
                return true;
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
      return false;
    }
  };

  /**
   * Eliminate backtracking
   * @note High space complexity
   */
  class EliminateBacktracking
      : public LLTable::template TransformPass<FirstSetGraph> {
    bool operator()(GrammarInfo& grammarInfo,
                    const FirstSetGraph& graph) override {
      auto& [grammar, createSubNonTerminal, _] = grammarInfo;
      auto& terminalNodeSet = graph.second;
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
          grammar.push_back(
              {left, {startNode->symbol, Symbol::createNonTerminal(newLeft)}});

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
              newRight.emplace_front(Symbol::NonTerminal, preNonTerminal);
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
              grammar.emplace_back(
                  newLeft,
                  std::list{extractStartNode.symbol,
                            Symbol::createNonTerminal(preNonTerminal)});
              grammar.emplace_back(newLeft,
                                   std::list{Symbol::createNonTerminal(left)});
              for (Edge& edge : edge->to->edges) {
                if (&edge != *it) {
                  edge.production->right.pop_front();
                  edge.production->right.emplace_front(Symbol::NonTerminal,
                                                       newLeft);
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
              newNextNodePath.extractCommonFactor(nextNodePath, grammar,
                                                  createSubNonTerminal);
              return true;
            }
            visitedAfterTerminal.insert(edge.to);
            traverseStack.push(edge.to);
            nextNodePath = newNextNodePath;
          }
        }
      }
      return false;
    }
  };
};
}  // namespace ParserGenerator