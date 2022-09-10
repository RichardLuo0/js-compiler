#pragma once

#include <cassert>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "Lexer.hpp"

template <>
struct std::hash<JsCompiler::TokenType> {
  size_t operator()(const JsCompiler::TokenType& terminal) const {
    return std::hash<int>{}(static_cast<int>(terminal));
  }
};

namespace LLTableGenerator {
typedef std::string NonTerminalType;
typedef JsCompiler::TokenType TerminalType;

class LLTable {
 public:
  struct Symbol {
    enum Type { Terminal, NonTerminal, End } type;
    std::variant<NonTerminalType, TerminalType> value;

   public:
    Symbol(NonTerminalType nonTerminal)
        : type(NonTerminal), value(nonTerminal) {}
    Symbol(TerminalType terminal) : type(Terminal), value(terminal) {}
    Symbol(Type type) : type(type) {}

    ~Symbol(){};

    NonTerminalType getNonTerminal() const {
      assert(type == NonTerminal);
      return std::get<NonTerminalType>(value);
    }

    TerminalType getTerminal() const {
      assert(type == Terminal);
      return std::get<TerminalType>(value);
    }

    constexpr bool operator==(const Symbol& another) const {
      if (type != another.type) {
        return false;
      } else if (type == Terminal || type == NonTerminal) {
        return value == another.value;
      } else {
        return true;
      }
    }

    constexpr bool operator!=(const Symbol& another) const {
      return !((*this) == another);
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
  struct HashPair {
    template <class T1>
    std::size_t operator()(const std::pair<T1, Symbol>& pair) const {
      return std::hash<T1>()(pair.first) ^ Symbol::Hash()(pair.second);
    }
  };

  Symbol start;
  std::unordered_map<std::pair<NonTerminalType, Symbol>, std::vector<Symbol>,
                     HashPair>
      table;

 public:
  LLTable();

  Symbol getStart() const { return start; }

  std::vector<Symbol> predict(const Symbol& currentSymbol,
                              const Symbol& nextInput) const noexcept(false) {
    assert(currentSymbol.type == Symbol::NonTerminal);
    std::pair<NonTerminalType, Symbol> key =
        std::make_pair(currentSymbol.getNonTerminal(), nextInput);
    if (!table.count(key)) throw std::out_of_range("No match prediction");
    return table.at(key);
  }
};
}  // namespace LLTableGenerator
