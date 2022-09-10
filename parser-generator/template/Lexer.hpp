#pragma once

#include <istream>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace GeneratedParser {
using TokenType = int;
static inline const TokenType eof = -1;
static inline const TokenType eol = -2;

struct Token {
  TokenType type{};
  std::string value;
};

class Lexer {
 protected:
  std::istream& stream;
  Token currentToken;

  std::vector<std::regex> tokenRegexList;

 public:
  explicit Lexer(std::istream& stream) : stream(stream){};

  void readNextToken() {
    for (int i = 0; i < tokenRegexList.size(); i++) {
      std::regex regex = tokenRegexList[i];
    }
  };

  [[nodiscard]] Token getCurrentToken() const { return currentToken; };
};
}  // namespace GeneratedParser
