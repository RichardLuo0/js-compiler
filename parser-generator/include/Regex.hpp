#pragma once

#include <exception>
#include <iterator>
#include <list>
#include <memory>
#include <stack>
#include <string>
#include <utility>

namespace ParserGenerator {
class Regex {
 protected:
  void parse(std::string regexStr);

 public:
  struct Transition;

  struct State {
    friend struct Transition;

   protected:
    std::list<Transition> transitionList;

   public:
    void addTransition(Transition transition);

    [[nodiscard]] std::list<const State*> accept(char ch) const;

    [[nodiscard]] bool isFinalState() const;
  };

  std::list<State> stateList;

  State& createState() {
    stateList.emplace_back();
    return stateList.back();
  }

  explicit Regex(std::string regexStr) { parse(std::move(regexStr)); };

  /**
   * @brief Build transition table
   */
  void build();

  bool match(const std::string& str);

  struct Condition {
    virtual bool operator()(char) const = 0;

    virtual ~Condition() = default;
  };

  struct AnyCondition : Condition {
    bool operator()(char) const override { return true; }
  };

  struct Transition {
    std::unique_ptr<Condition> condition;
    const State* state;
  };

  struct Token {
   public:
    virtual State& generate(Regex& regex, State& preState) const = 0;
  };

  struct Container : Token {
   public:
    /**
     * @param  token   : Next token.
     * @return {bool}  : Should be finished after push or not.
     */
    virtual bool push(std::unique_ptr<Token> token) = 0;

    virtual std::unique_ptr<Token> pop() = 0;

    [[nodiscard]] virtual size_t size() const = 0;
  };

  struct Char : Token, Condition {
   public:
    const char value;

    explicit Char(char value) : value(value){};

    State& generate(Regex& regex, State& preState) const override;

    bool operator()(char ch) const override;
  };

  struct Any : Token {
   public:
    State& generate(Regex& regex, State& preState) const override;
  };

  struct Group : Container {
   protected:
    std::list<std::unique_ptr<Token>> tokenList;

   public:
    State& generate(Regex& regex, State& preState) const override;

    bool push(std::unique_ptr<Token> token) override;

    std::unique_ptr<Token> pop() override;

    [[nodiscard]] size_t size() const override;
  };

  struct CharSet : Container, Condition {
   protected:
    std::list<Char> charList;

   public:
    bool isInverted = false;

    CharSet() = default;
    ;
    CharSet(std::list<Char> charList, bool isInverted)
        : charList(std::move(charList)), isInverted(isInverted){};

    State& generate(Regex& regex, State& preState) const override;

    bool push(std::unique_ptr<Token> token) override;

    std::unique_ptr<Token> pop() override;

    [[nodiscard]] size_t size() const override;

    bool operator()(char ch) const override;
  };

  struct Alternation : Container {
   protected:
    const std::unique_ptr<Token> left;
    std::unique_ptr<Token> right;

   public:
    explicit Alternation(std::unique_ptr<Token> left,
                         std::unique_ptr<Token> right = nullptr)
        : left(std::move(left)), right(std::move(right)) {}

    State& generate(Regex& regex, State& preState) const override;

    bool push(std::unique_ptr<Token> token) override;

    std::unique_ptr<Token> pop() override;

    [[nodiscard]] size_t size() const override;
  };

  struct ZeroOrMore : Token {
   protected:
    std::unique_ptr<Token> token;

   public:
    explicit ZeroOrMore(std::unique_ptr<Token> token)
        : token(std::move(token)) {}

    State& generate(Regex& regex, State& preState) const override;
  };

  struct ZeroOrOnce : Token {
   protected:
    std::unique_ptr<Token> token;

   public:
    explicit ZeroOrOnce(std::unique_ptr<Token> token)
        : token(std::move(token)) {}

    State& generate(Regex& regex, State& preState) const override;
  };
};
}  // namespace ParserGenerator