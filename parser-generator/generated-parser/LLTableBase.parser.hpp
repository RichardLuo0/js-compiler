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
   public:
    enum Type { Terminal, NonTerminal, End } type;

   protected:
    std::variant<NonTerminalType, TerminalType> value;

   public:
    explicit Symbol(NonTerminalType nonTerminal)
        : type(NonTerminal), value(nonTerminal) {}
    explicit Symbol(TerminalType terminal) : type(Terminal), value(terminal) {}
    explicit Symbol(Type type) : type(type) {}
    // Only used for serialization
    Symbol() = default;

    [[nodiscard]] const NonTerminalType& getNonTerminal() const {
      assert(type == NonTerminal);
      return std::get<NonTerminalType>(value);
    }

    [[nodiscard]] const TerminalType& getTerminal() const {
      assert(type == Terminal);
      return std::get<TerminalType>(value);
    }

    constexpr bool operator==(const Symbol& another) const {
      return type == another.type && value == another.value;
    }

    constexpr bool operator!=(const Symbol& another) const {
      return !((*this) == another);
    }

    struct Hash {
      size_t operator()(const Symbol& symbol) const {
        return std::hash<Type>()(symbol.type) ^
               std::hash<std::variant<NonTerminalType, TerminalType>>{}(
                   symbol.value);
      }
    };
  };

  static inline const Symbol END{Symbol::End};

 protected:
  const Symbol start;
  std::unordered_map<
      NonTerminalType,
      std::unordered_map<Symbol, std::list<Symbol>, typename Symbol::Hash>>
      table;

 public:
  explicit LLTable(NonTerminalType start) : start(std::move(start)) {}

  const Symbol& getStart() const { return start; }
};
}  // namespace GeneratedParser
