#pragma once

#include <istream>
#include <memory>
#include <string>

namespace ParserGenerator {
enum TokenType {
  Eof,
  Definition,      // =
  Termination,     // ;
  Alternation,     // |
  NonTerminal,     // Any char but token,
  StringTerminal,  // ".*"
  RegexTerminal,   // /Regex/
  Epsilon,         // ""
  Comment          // (* *)
};

struct Token {
  TokenType type = Eof;
  std::string value;
};

class Lexer {
 protected:
  std::istream &stream;
  Token currentToken;

 public:
  explicit Lexer(std::istream &stream) : stream(stream){};

  virtual void readNextToken() noexcept(false) = 0;

  [[nodiscard]] Token getCurrentToken() const { return currentToken; };
};

class BNFLexer : public Lexer {
 public:
  explicit BNFLexer(std::istream &stream) : Lexer(stream){};

  static inline std::unique_ptr<BNFLexer> create(std::istream &stream) {
    return std::make_unique<BNFLexer>(stream);
  }

  void readNextToken() noexcept(false) override;
};
}  // namespace ParserGenerator
