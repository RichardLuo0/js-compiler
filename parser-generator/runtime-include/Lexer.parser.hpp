#pragma once

#include <algorithm>
#include <istream>
#include <iterator>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Regex.parser.hpp"
#include "Utility.parser.hpp"

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

  struct MatchState;
  struct Matcher {
    [[nodiscard]] virtual bool match(Stream&, MatchState&) = 0;
  };

  struct StringMatcher : public Matcher {
   protected:
    const std::string str;

   public:
    explicit StringMatcher(std::string str) : str(std::move(str)){};

    [[nodiscard]] bool match(Stream& stream, MatchState&) override {
      for (const char& ch : str) {
        if (stream.get() != ch) return false;
      }
      return true;
    }
  };

  struct RegexMatcher : public Matcher {
   protected:
    const Regex regex;

   public:
    explicit RegexMatcher(const std::string& regexStr)
        : regex(Regex(regexStr)){};

    [[nodiscard]] bool match(Stream& stream, MatchState&) override {
      return regex.match(stream);
    }
  };

  struct RegexExcludeMatcher : public Matcher {
   protected:
    const Regex regex;
    const std::vector<size_t> excludeList;

   public:
    RegexExcludeMatcher(const std::string& regexStr,
                        std::vector<size_t>&& excludeList)
        : regex(Regex(regexStr)), excludeList(excludeList){};

    [[nodiscard]] bool match(Stream& stream, MatchState& state) override {
      size_t pos = stream.tellg();
      if (regex.match(stream)) {
        size_t regexEndPos = stream.tellg();
        bool isNotExcluded =
            std::find_if(excludeList.begin(), excludeList.end(),
                         [&state, &stream, &pos](size_t index) {
                           stream.seekg(pos);
                           return state.match(index, stream);
                         }) == excludeList.end();
        stream.seekg(regexEndPos);
        return isNotExcluded;
      }
      return false;
    }
  };

  struct MatchState {
   protected:
    std::unordered_map<size_t, bool> cache;
    const std::vector<std::unique_ptr<Matcher>>& matcherList;

   public:
    explicit MatchState(
        const std::vector<std::unique_ptr<Matcher>>& matcherList)
        : matcherList(matcherList) {}

    bool match(size_t index, Stream& stream) {
      if (!cache.contains(index))
        cache[index] = matcherList.at(index)->match(stream, *this);
      return cache.at(index);
    }
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

    MatchState state(matcherList);
    size_t startPos = stream.tellg();
    bool isMatched = false;
    for (const size_t& index : matcherIndexIterable) {
      if (state.match(index, stream)) {
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
};
}  // namespace GeneratedParser
