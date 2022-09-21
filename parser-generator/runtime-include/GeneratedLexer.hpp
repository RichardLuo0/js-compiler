#pragma once

#include <istream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Regex.hpp"

namespace GeneratedParser {
using TokenType = int;
static inline const TokenType eof = -1;

struct Token {
  TokenType type = -1;
  std::string value;
};

class Lexer {
 protected:
  std::istream& stream;
  Token currentToken;

  struct TokenMatcher {
    [[nodiscard]] virtual bool match(char) = 0;
    virtual void reset() = 0;
    virtual std::string toString() = 0;
  };

  struct RegexMatcher : public TokenMatcher {
   protected:
    const std::string regexStr;
    const Regex regex;
    std::unordered_set<const Regex::State*> currentState;

   public:
    explicit RegexMatcher(const std::string& regexStr)
        : regexStr(regexStr), regex(Regex(regexStr)) {
      currentState.insert(&regex.getStartState());
    };

    [[nodiscard]] bool match(char ch) override {
      if (currentState.empty()) return false;
      if ((ch == EOF || !regex.isGreedy) &&
          Regex::isAnyStateMatch(currentState))
        return true;
      currentState = Regex::matchCharFromState(currentState, ch);
      if (regex.isGreedy)
        return currentState.size() == 1 &&
               (*currentState.begin())->isFinalState();
      return false;
    }

    void reset() override {
      currentState.clear();
      currentState.insert(&regex.getStartState());
    }

    std::string toString() override { return regexStr; }
  };

  struct StringMatcher : public TokenMatcher {
   protected:
    const std::string str;
    int index = 0;

   public:
    explicit StringMatcher(std::string str) : str(std::move(str)){};

    [[nodiscard]] bool match(char ch) override {
      if (index <= -1 || ch == EOF) return false;
      if (index >= static_cast<int>(str.size())) return true;
      if (str[index++] != ch) index = -1;
      return false;
    }

    void reset() override { index = 0; }

    std::string toString() override { return str; }
  };

  std::vector<std::unique_ptr<TokenMatcher>> matcherList;

  [[nodiscard]] virtual inline bool isEof(const char& ch) const {
    return ch == EOF;
  }

  [[nodiscard]] virtual inline bool isSpace(const char& ch) const {
    return std::isspace(ch);
  }

 public:
  static std::unique_ptr<Lexer> create(std::istream& stream) {
    return std::make_unique<Lexer>(stream);
  }

  explicit Lexer(std::istream& stream);

  void readNextToken() {
    static char currentChar = ' ';
    if (isEof(currentChar)) {
      currentToken = {eof, ""};
      return;
    }

    while (isSpace(currentChar)) {
      currentChar = static_cast<char>(stream.get());
    }

    // Look ahead
    std::string value;
    bool isMatched = false;
    while (!isMatched) {
      for (size_t i = 0; i < matcherList.size(); i++) {
        auto& matcher = matcherList[i];
        if (matcher->match(currentChar)) {
          isMatched = true;
          currentToken = {static_cast<TokenType>(i), value};
          value.clear();
          break;
        }
      }
      if (isMatched) break;
      if (isEof(currentChar)) throw std::runtime_error("Incomplete token");
      value += currentChar;
      currentChar = static_cast<char>(stream.get());
    }

    for (auto& matcher : matcherList) {
      matcher->reset();
    }
  };

  [[nodiscard]] Token getCurrentToken() const { return currentToken; };

  [[nodiscard]] inline std::string getMatcherStringFromIndex(
      size_t index) const {
    return matcherList[index]->toString();
  }
};
}  // namespace GeneratedParser
