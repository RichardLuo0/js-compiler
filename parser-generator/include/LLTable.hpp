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

  using CreateSubNonTerminalType =
      std::function<NonTerminalType(NonTerminalType)>;

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

    // Only used for debug print; Require NonTerminalType and TerminalType can
    // be std::to_string()
    [[nodiscard]] std::string toString() const {
      std::string str;
      str += left + ", ";
      for (auto& symbol : right) {
        switch (symbol.type) {
          case Symbol::Terminal:
            str += "\"" + std::to_string(symbol.getTerminal()) + "\"";
            break;
          case Symbol::NonTerminal:
            str += std::to_string(symbol.getNonTerminal());
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
  };

  struct Node;
  struct Edge {
    Production* production;
    Node* to;
  };

  struct Node {
    std::list<Edge> edges;
    Symbol symbol = LLTableBase::END;
  };

  using FirstSetGraph =
      std::pair<std::unordered_map<Symbol, Node, typename Symbol::Hash>,
                std::unordered_set<Node*>>;

  struct GrammarInfo {
    std::list<Production>& grammar;
    const CreateSubNonTerminalType& createSubNonTerminalType;
    const Symbol& start;
  };

  template <class AnalysisResultType>
  struct AnalysisPass {
    virtual void operator()(const GrammarInfo&, AnalysisResultType&) = 0;
  };

  template <class AnalysisResultType>
  struct TransformPass {
    /**
     * @return {bool} : Indicating if the grammar is changed.
     */
    virtual bool operator()(GrammarInfo&, const AnalysisResultType&) = 0;
  };

  struct OptimizationPass {
    virtual void operator()(GrammarInfo&) = 0;
  };

 protected:
  void debugPrint(const std::list<Production>& grammar) {
    static size_t i = 0;
    std::ofstream f(".log/log" + std::to_string(i++) + ".txt");
    for (auto& production : grammar) {
      f << production.toString() << std::endl;
    }
  }

  std::list<std::unique_ptr<OptimizationPass>> optimizationPassList;
  std::unique_ptr<AnalysisPass<FirstSetGraph>> firstSetAnalysisPass;
  std::list<std::unique_ptr<TransformPass<FirstSetGraph>>> transformPassList;

  auto transformToLLGrammar(
      std::list<Production>& grammar,
      const CreateSubNonTerminalType& createSubNonTerminalType) {
    // A graph to represent the first set. The first of the pair is used to
    // store all nodes, the second of the pair is used to record terminal nodes.
    FirstSetGraph graph;

    // Fixed point iteration
    bool isChanged = false;
    GrammarInfo grammarInfo{grammar, createSubNonTerminalType, this->start};
    do {
      isChanged = false;
      for (auto& optimizationPass : optimizationPassList) {
        optimizationPass->operator()(grammarInfo);
      }
      firstSetAnalysisPass->operator()(grammarInfo, graph);
      for (auto& transformPass : transformPassList) {
        while (transformPass->operator()(grammarInfo, graph)) {
          isChanged = true;
          firstSetAnalysisPass->operator()(grammarInfo, graph);
        }
      }
    } while (isChanged);

    return graph;
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

  std::list<Production> grammar;
  const CreateSubNonTerminalType& createSubNonTerminal;

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
          CreateSubNonTerminalType createSubNonTerminal)
      : LLTableBase(std::move(start)),
        grammar(grammar),
        createSubNonTerminal(createSubNonTerminal) {}
  LLTable(NonTerminalType start, CreateSubNonTerminalType createSubNonTerminal)
      : LLTableBase(std::move(start)),
        createSubNonTerminal(createSubNonTerminal) {}

  void setGrammar(const std::list<Production>& grammar) {
    this->grammar = grammar;
  }

  template <class PassType>
    requires std::is_base_of<OptimizationPass, PassType>::value
  auto& add() {
    optimizationPassList.push_back(std::make_unique<PassType>());
    return *this;
  }

  template <class PassType>
    requires std::is_base_of<TransformPass<FirstSetGraph>, PassType>::value
  auto& add() {
    transformPassList.push_back(std::make_unique<PassType>());
    return *this;
  }

  template <class FirstSetAnalysisPassType>
  auto& setFirstSetAnalysisPass() {
    firstSetAnalysisPass = std::make_unique<FirstSetAnalysisPassType>();
    return *this;
  }

  void build() {
    const auto& graph = transformToLLGrammar(grammar, createSubNonTerminal);
    createFirstSet(graph.second);
    createFollowSet(grammar);
  }

  const auto& getTable() const { return this->table; }
};
}  // namespace ParserGenerator
