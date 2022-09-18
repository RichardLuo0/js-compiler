#pragma once

#include <cassert>
#include <list>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace GeneratedParser {
/*
 * The specialization of std::hash<NonTerminalType> and std::hash<TerminalType>
 * must be defined
 */
template <typename NonTerminalType, typename TerminalType>
class LLTable {
 public:
  struct Symbol {
    enum Type { Terminal, NonTerminal, End } type;
    std::variant<NonTerminalType, TerminalType> value;

   public:
    explicit Symbol(NonTerminalType nonTerminal)
        : type(NonTerminal), value(nonTerminal) {}
    explicit Symbol(TerminalType terminal) : type(Terminal), value(terminal) {}
    explicit Symbol(Type type) : type(type) {}

    [[nodiscard]] NonTerminalType getNonTerminal() const {
      assert(type == NonTerminal);
      return std::get<NonTerminalType>(value);
    }

    [[nodiscard]] TerminalType getTerminal() const {
      assert(type == Terminal);
      return std::get<TerminalType>(value);
    }

    constexpr bool operator==(const Symbol& another) const {
      if (type != another.type) {
        return false;
      }
      return value == another.value;
    }

    constexpr bool operator!=(const Symbol& another) const {
      return (*this) != another;
    }

    struct Hash {
      size_t operator()(const Symbol& symbol) const {
        return std::hash<std::variant<NonTerminalType, TerminalType>>{}(
            symbol.value);
      }
    };
  };

  static inline const Symbol end{Symbol::End};

 protected:
  struct PairHash {
    template <class T1>
    std::size_t operator()(const std::pair<T1, Symbol>& pair) const {
      return std::hash<T1>()(pair.first) ^ typename Symbol::Hash()(pair.second);
    }
  };

  Symbol start;
  // Key is hash(non-terminal + terminal), value is production
  std::unordered_map<std::pair<NonTerminalType, Symbol>, std::list<Symbol>,
                     PairHash>
      table;

 public:
  explicit LLTable(NonTerminalType start) : start(start) {}

  Symbol getStart() const { return start; }

  std::list<Symbol> predict(const Symbol& currentSymbol,
                            const Symbol& nextInput) const noexcept(false) {
    assert(currentSymbol.type == Symbol::NonTerminal);
    auto key = std::make_pair(currentSymbol.getNonTerminal(), nextInput);
    if (!table.count(key)) throw std::runtime_error("No match prediction");
    return table.at(key);
  }
};
}  // namespace GeneratedParser
