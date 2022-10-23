#pragma once

#include <algorithm>
#include <istream>
#include <iterator>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "Regex.parser.hpp"
#include "Serializer.parser.hpp"
#include "Utility.parser.hpp"

namespace GeneratedParser {
using TokenType = int;
static inline const TokenType Eof = -1;

struct Token {
  TokenType type = -1;
  std::string value;
};

class Lexer {
 protected:
  struct Matcher;

 public:
  friend class Serializer::Serializer<
      std::vector<std::unique_ptr<Lexer::Matcher>>>;
  friend class Parser;

 protected:
  using Stream = Utility::ForwardBufferedInputStream;

  Stream stream;
  Token currentToken;

  struct MatchState;
  struct Matcher {
    virtual ~Matcher() = default;

    [[nodiscard]] virtual bool match(Stream&, MatchState&) = 0;
  };

  struct StringMatcher : public Matcher {
   protected:
    const std::string_view str;

   public:
    explicit StringMatcher(const std::string_view str) : str(str){};

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
    explicit RegexMatcher(const std::string_view& regexStr)
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
    RegexExcludeMatcher(std::string_view regexStr,
                        std::vector<size_t> excludeList)
        : regex(Regex(regexStr)), excludeList(std::move(excludeList)){};

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
    std::unordered_map<size_t, int> cache;
    const std::vector<std::unique_ptr<Matcher>>& matcherList;

   public:
    explicit MatchState(
        const std::vector<std::unique_ptr<Matcher>>& matcherList)
        : matcherList(matcherList) {}

    bool match(size_t index, Stream& stream) {
      if (!cache.contains(index)) {
        if (matcherList.at(index)->match(stream, *this))
          cache.emplace(index, static_cast<int>(stream.tellg()));
        else
          cache.emplace(index, -1);
      } else
        stream.seekg(cache.at(index));
      return cache.at(index) > -1;
    }

    int getMatchedPos() {
      auto it = std::find_if(cache.begin(), cache.end(),
                             [](std::pair<size_t, int> singleMatch) {
                               return singleMatch.second > -1;
                             });
      return it != cache.end() ? it->second : -1;
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

  explicit Lexer(std::istream& stream) : stream(stream) {}

  template <class Iterable>
  void readNextTokenExpect(Iterable matcherIndexIterable) {
    if (stream.peek() == EOF) {
      currentToken = {Eof, ""};
      return;
    }

    while (isSpace(static_cast<char>(stream.peek()))) {
      stream.read();
    }
    stream.shrinkBufferToIndex();

    MatchState state(matcherList);
    size_t startPos = stream.tellg();
    for (const size_t& index : matcherIndexIterable) {
      if (state.match(index, stream))
        currentToken = {static_cast<TokenType>(index),
                        stream.getBufferToIndexAsString()};
      stream.seekg(startPos);
    }
    int matchedPos = state.getMatchedPos();
    if (matchedPos == -1) throw std::runtime_error("Unexpected token");
    stream.seekg(matchedPos);

    stream.shrinkBufferToIndex();
  }

  void readNextTokenExpectEof() {
    if (stream.peek() == EOF) {
      currentToken = {Eof, ""};
      return;
    }
    throw std::runtime_error("Expecting EOF but get " +
                             std::to_string(stream.peek()));
  }

  [[nodiscard]] const Token& getCurrentToken() const { return currentToken; };
};

template <>
class Serializer::Serializer<std::vector<std::unique_ptr<Lexer::Matcher>>>
    : ISerializer {
 protected:
  std::vector<std::unique_ptr<Lexer::Matcher>>& matcherList;

 public:
  explicit Serializer(std::vector<std::unique_ptr<Lexer::Matcher>>& matcherList)
      : matcherList(matcherList) {}

  void deserialize(BinaryIfStream& stream) override {
    size_t size;
    Serializer<size_t>(size).deserialize(stream);
    matcherList.resize(size);
    size_t i = 0;
    while (stream.peek() != EOS) {
      stream.get();
      BinaryIType type = stream.get();
      switch (type) {
        case 0: {
          std::string_view sv;
          Serializer<std::string_view>(sv).deserialize(stream);
          matcherList[i++] = std::make_unique<Lexer::StringMatcher>(sv);
          break;
        }
        case 1: {
          std::string_view sv;
          Serializer<std::string_view>(sv).deserialize(stream);
          matcherList[i++] = std::make_unique<Lexer::RegexMatcher>(sv);
          break;
        }
        case 2: {
          std::string_view regex;
          Serializer<std::string_view>(regex).deserialize(stream);
          std::vector<size_t> excludeList;
          Serializer<std::vector<size_t>>(excludeList).deserialize(stream);
          matcherList[i++] =
              std::make_unique<Lexer::RegexExcludeMatcher>(regex, excludeList);
          break;
        }
        default:
          throw std::runtime_error("Unknow symbol type: " +
                                   std::to_string(type));
      }
    }
    stream.get();
  };
};
}  // namespace GeneratedParser
