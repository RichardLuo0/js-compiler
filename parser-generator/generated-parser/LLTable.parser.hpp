#pragma once

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "LLTableBase.parser.hpp"
#include "Serializer.parser.hpp"

namespace GeneratedParser {
class GeneratedLLTable : public LLTable<std::string_view, size_t> {
  friend class Parser;

 protected:
  std::unordered_map<std::string_view, std::list<size_t>> candidateCache;

 public:
  GeneratedLLTable() : LLTable("Start"){};

  const std::list<size_t>& getCandidate(const std::string_view& nonTerminal) {
    if (!candidateCache.contains(nonTerminal)) {
      auto& candidateCacheOfNonTerminal = candidateCache[nonTerminal];
      for (const auto& [symbol, _] : table.at(nonTerminal)) {
        if (symbol.type == Symbol::Terminal)
          candidateCacheOfNonTerminal.push_back(symbol.getTerminal());
      }
    }
    return candidateCache.at(nonTerminal);
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

template <>
class Serializer::Serializer<GeneratedLLTable::Symbol> : public ISerializer {
 protected:
  using Symbol = GeneratedLLTable::Symbol;

  Symbol& symbol;

 public:
  explicit Serializer(Symbol& symbol) : symbol(symbol) {}
  explicit Serializer(const Symbol& symbol)
      : symbol(const_cast<Symbol&>(symbol)) {}

  void deserialize(BinaryIfstream& stream) override {
    BinaryIType type = stream.get();
    switch (type) {
      case GeneratedLLTable::Symbol::Terminal: {
        size_t terminal = 0;
        Serializer<size_t>(terminal).deserialize(stream);
        symbol = Symbol{terminal};
        break;
      }
      case GeneratedLLTable::Symbol::NonTerminal: {
        std::string_view sv;
        Serializer<std::string_view>(sv).deserialize(stream);
        symbol = Symbol{sv};
        break;
      }
      case GeneratedLLTable::Symbol::End:
        symbol = GeneratedLLTable::END;
        break;
      default:
        throw std::runtime_error("Unknow symbol type: " + std::to_string(type));
    }
    if (stream.get() != EOS) throw std::runtime_error("Incomplete symbol");
  }
};
}  // namespace GeneratedParser
