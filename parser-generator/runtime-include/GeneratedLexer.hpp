#pragma once

#include <ios>
#include <istream>
#include <iterator>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Regex.hpp"
#include "RuntimeUtility.hpp"

namespace GeneratedParser {
using TokenType = int;
static inline const TokenType eof = -1;

struct Token {
  TokenType type = -1;
  std::string value;
};

class Lexer {
 protected:
  using Stream = Utility::ForwardBufferedInputStream;

  Stream stream;
  Token currentToken;

  struct Matcher {
    [[nodiscard]] virtual bool match(Stream&) = 0;

    virtual std::string toString() = 0;
  };

  struct RegexMatcher : public Matcher {
   protected:
    const std::string regexStr;
    const Regex regex;

   public:
    explicit RegexMatcher(const std::string& regexStr)
        : regexStr(regexStr), regex(Regex(regexStr)){};

    [[nodiscard]] bool match(Stream& stream) override {
      return regex.match(stream);
    }

    std::string toString() override { return regexStr; }
  };

  struct StringMatcher : public Matcher {
   protected:
    const std::string str;

   public:
    explicit StringMatcher(std::string str) : str(std::move(str)){};

    [[nodiscard]] bool match(Stream& stream) override {
      for (const char& ch : str) {
        if (stream.get() != ch) return false;
      }
      return true;
    }

    std::string toString() override { return str; }
  };

  std::vector<std::unique_ptr<Matcher>> matcherList;

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

  template <class Iterable>
  void readNextTokenExpect(const Iterable& matcherIndexIterable) {
    if (stream.peek() == EOF) {
      currentToken = {eof, ""};
      return;
    }

    while (isSpace(static_cast<char>(stream.peek()))) {
      stream.read();
    }
    stream.shrinkBufferToIndex();

    size_t startPos = stream.tellg();
    bool isMatched = false;
    for (const size_t& index : matcherIndexIterable) {
      auto& matcher = matcherList[index];
      if (matcher->match(stream)) {
        isMatched = true;
        currentToken = {static_cast<TokenType>(index),
                        stream.getBufferToIndexAsString()};
        break;
      }
      stream.seekg(startPos);
    }
    if (!isMatched) throw std::runtime_error("Incomplete token");

    stream.shrinkBufferToIndex();
  }

  void readNextTokenExpectEof() {
    if (stream.peek() == EOF) {
      currentToken = {eof, ""};
      return;
    }
    throw std::runtime_error("Expecting EOF but get " +
                             std::to_string(stream.peek()));
  }

  [[nodiscard]] Token getCurrentToken() const { return currentToken; };

  [[nodiscard]] inline std::string getMatcherStringFromIndex(
      size_t index) const {
    return matcherList.at(index)->toString();
  }
};
}  // namespace GeneratedParser
