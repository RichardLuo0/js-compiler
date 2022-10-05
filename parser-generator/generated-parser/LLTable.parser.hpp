#pragma once

#include <algorithm>
#include <ranges>
#include <unordered_map>

#include "LLTableBase.parser.hpp"

namespace GeneratedParser {
class GeneratedLLTable : public LLTable<std::string, size_t> {
 protected:
  std::unordered_map<std::string, std::list<size_t>> cache;

 public:
  GeneratedLLTable() : LLTable("Start") {
    generateTable();
    for (const auto& [nonTerminal, map] : table) {
      const auto& leftMap = table.at(nonTerminal);
      auto& leftCache = cache[nonTerminal];
      for (const auto& [symbol, _] : leftMap) {
        if (symbol.type == Symbol::Terminal)
          leftCache.push_back(symbol.getTerminal());
      }
    }
  };

  void generateTable();

  const std::list<size_t>& getCandidate(const std::string& nonTerminal) const {
    return cache.at(nonTerminal);
  }

  std::list<Symbol> predict(const Symbol& currentSymbol,
                            const Symbol& nextInput) const noexcept(false) {
    assert(currentSymbol.type == Symbol::NonTerminal);
    const auto& leftMap = table.at(currentSymbol.getNonTerminal());
    if (!leftMap.contains(nextInput))
      throw std::runtime_error("No match prediction");
    return leftMap.at(nextInput);
  }
};
}  // namespace GeneratedParser
