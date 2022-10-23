#pragma once

#include <cassert>
#include <list>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <type_traits>
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
class LLTableBase {
 protected:
  struct VariantSymbol {
   public:
    enum Type { Terminal, NonTerminal, End } type;

   protected:
    std::variant<NonTerminalType, TerminalType> value;

   public:
    explicit VariantSymbol(NonTerminalType nonTerminal)
        : type(NonTerminal), value(nonTerminal) {}
    explicit VariantSymbol(TerminalType terminal)
        : type(Terminal), value(terminal) {}
    explicit VariantSymbol(Type type) : type(type) {}
    // Only used for serialization
    VariantSymbol() = default;

    static inline VariantSymbol createNonTerminal(NonTerminalType value) {
      return VariantSymbol(value);
    }

    static inline VariantSymbol createTerminal(TerminalType value) {
      return VariantSymbol(value);
    }

    [[nodiscard]] const NonTerminalType& getNonTerminal() const {
      assert(type == NonTerminal);
      return std::get<NonTerminalType>(value);
    }

    [[nodiscard]] const TerminalType& getTerminal() const {
      assert(type == Terminal);
      return std::get<TerminalType>(value);
    }

    constexpr bool operator==(const VariantSymbol& another) const {
      return type == another.type && value == another.value;
    }

    constexpr bool operator!=(const VariantSymbol& another) const {
      return !((*this) == another);
    }

    struct Hash {
      size_t operator()(const VariantSymbol& symbol) const {
        return std::hash<Type>()(symbol.type) ^
               std::hash<std::variant<NonTerminalType, TerminalType>>{}(
                   symbol.value);
      }
    };
  };

  struct SameTypeSymbol {
   public:
    enum Type { Terminal, NonTerminal, End } type;

   protected:
    NonTerminalType value;

   public:
    explicit SameTypeSymbol(Type type, NonTerminalType value)
        : type(type), value(value) {}
    explicit SameTypeSymbol(Type type) : type(type) {}
    // Only used for serialization
    SameTypeSymbol() = default;

    static inline SameTypeSymbol createNonTerminal(NonTerminalType value) {
      return SameTypeSymbol(NonTerminal, value);
    }

    static inline SameTypeSymbol createTerminal(NonTerminalType value) {
      return SameTypeSymbol(Terminal, value);
    }

    [[nodiscard]] const NonTerminalType& getNonTerminal() const {
      assert(type == NonTerminal);
      return value;
    }

    [[nodiscard]] const TerminalType& getTerminal() const {
      assert(type == Terminal);
      return value;
    }

    constexpr bool operator==(const SameTypeSymbol& another) const {
      return type == another.type && value == another.value;
    }

    constexpr bool operator!=(const SameTypeSymbol& another) const {
      return !((*this) == another);
    }

    struct Hash {
      size_t operator()(const SameTypeSymbol& symbol) const {
        return std::hash<Type>()(symbol.type) ^
               std::hash<NonTerminalType>{}(symbol.value);
      }
    };
  };

 public:
  using Symbol =
      std::conditional<std::is_same<NonTerminalType, TerminalType>::value,
                       SameTypeSymbol, VariantSymbol>::type;

  static inline const Symbol END{Symbol::End};

 protected:
  NonTerminalType start;
  std::unordered_map<
      NonTerminalType,
      std::unordered_map<Symbol, std::list<Symbol>, typename Symbol::Hash>>
      table;

 public:
  LLTableBase() = default;
  explicit LLTableBase(NonTerminalType start) : start(std::move(start)) {}

  const NonTerminalType& getStart() const { return start; }
};
}  // namespace GeneratedParser
