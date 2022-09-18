#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <stack>
#include <string>
#include <unordered_set>
#include <utility>

namespace GeneratedParser {
class Regex {
 protected:
  void parse(std::string regexStr) {
    std::stack<std::unique_ptr<Container>> stack;
    stack.push(std::make_unique<Group>());

    auto popLastToken = [&stack]() -> std::unique_ptr<Token> {
      if (stack.top()->size() > 0) return stack.top()->pop();

      auto topContainer = std::move(stack.top());
      stack.pop();
      return topContainer;
    };

    std::function<void(size_t)> finishTopContainer = nullptr;
    std::function<void(size_t, std::unique_ptr<Token>)> pushToTopContainer =
        nullptr;

    finishTopContainer = [&stack, &pushToTopContainer](size_t i) {
      if (stack.size() < 2)
        throw std::runtime_error("No group to close at " + std::to_string(i));
      std::unique_ptr<Container> current = std::move(stack.top());
      stack.pop();
      pushToTopContainer(i, std::move(current));
    };

    pushToTopContainer = [&stack, &finishTopContainer](
                             size_t i, std::unique_ptr<Token> token) {
      bool shouldBeFinished = stack.top()->push(std::move(token));
      if (shouldBeFinished) finishTopContainer(i);
    };

    for (size_t i = 0; i < regexStr.size(); i++) {
      char &ch = regexStr[i];
      switch (ch) {
        case '/':  // Delimiter
          if (i == regexStr.size() - 2) {
            if (regexStr[++i] == 'U') isGreedy = false;
          }
          break;
        case '(':
          stack.push(std::make_unique<Group>());
          break;
        case ')':
          finishTopContainer(i);
          break;
        case '^': {
          auto lastToken = popLastToken();
          auto *charSet = dynamic_cast<CharSet *>(lastToken.release());
          if (charSet == nullptr || charSet->size() != 0)
            throw std::runtime_error("^ must appear right after [ at " +
                                     std::to_string(i));
          charSet->isInverted = true;
          stack.push(std::unique_ptr<CharSet>(charSet));
          break;
        }
        case '[':
          stack.push(std::make_unique<CharSet>());
          break;
        case ']':
          finishTopContainer(i);
          break;
        case '|': {
          auto alternation = std::make_unique<Alternation>(popLastToken());
          stack.push(std::move(alternation));
          break;
        }
        case '.':
          pushToTopContainer(i, std::make_unique<Any>());
          break;
        case '*': {
          auto zeroOrMore = std::make_unique<ZeroOrMore>(popLastToken());
          pushToTopContainer(i, std::move(zeroOrMore));
          break;
        }
        case '?': {
          auto zeroOrOnce = std::make_unique<ZeroOrOnce>(popLastToken());
          pushToTopContainer(i, std::move(zeroOrOnce));
          break;
        }
        case '\\':
          ch = regexStr[++i];
        default:
          pushToTopContainer(i, std::make_unique<Char>(ch));
      }
    }

    // Generate epsilon NFA
    if (stack.size() != 1)
      throw std::runtime_error("A group or char class is not closed");
    stack.top()->generate(*this, createState());
  };

 public:
  struct Transition;
  struct State {
    friend struct Transition;

   protected:
    std::list<Transition> transitionList;

   public:
    void addTransition(Transition transition) {
      transitionList.push_back(std::move(transition));
    };

    [[nodiscard]] std::unordered_set<const State *> accept(char ch) const {
      std::unordered_set<const State *> stateSet;
      for (const auto &transition : transitionList) {
        if (transition.condition) {
          if (transition.condition->operator()(ch))
            stateSet.insert(transition.state);
        } else {
          auto nextSet = transition.state->accept(ch);
          stateSet.insert(nextSet.begin(), nextSet.end());
        }
      }
      return stateSet;
    };

    [[nodiscard]] bool isFinalState() const {
      if (transitionList.empty()) return true;
      return std::find_if(transitionList.begin(), transitionList.end(),
                          [](const Transition &transition) {
                            return transition.condition != nullptr ||
                                   !transition.state->isFinalState();
                          }) == transitionList.end();
    };
  };

  std::list<State> stateList;

  bool isGreedy = true;

  explicit Regex(std::string regexStr) { parse(std::move(regexStr)); };

  State &createState() {
    stateList.emplace_back();
    return stateList.back();
  }

  [[nodiscard]] const State &getStartState() const { return stateList.front(); }

  [[nodiscard]] static std::unordered_set<const Regex::State *>
  matchCharFromState(const std::unordered_set<const Regex::State *> &state,
                     char ch) {
    std::unordered_set<const Regex::State *> currentState;
    for (const auto &state : state) {
      if (state->isFinalState())
        currentState.insert(state);
      else {
        auto nextSet = state->accept(ch);
        currentState.insert(nextSet.begin(), nextSet.end());
      }
    }
    return currentState;
  }

  static bool isAnyStateMatch(
      std::unordered_set<const Regex::State *> currentState) {
    return std::find_if(currentState.begin(), currentState.end(),
                        [](const State *state) {
                          return state->isFinalState();
                        }) != currentState.end();
  }

  bool match(const std::string &str) {
    std::unordered_set<const Regex::State *> currentState{&getStartState()};
    for (const auto &ch : str) {
      currentState = matchCharFromState(currentState, ch);
      if (!isGreedy && isAnyStateMatch(currentState)) return true;
    }
    return isGreedy ? isAnyStateMatch(currentState) : false;
  }

  struct Condition {
    virtual bool operator()(char) const = 0;

    virtual ~Condition() = default;
  };

  struct Transition {
    std::unique_ptr<Condition> condition;
    const State *state;
  };

  struct AnyCondition : Condition {
    bool operator()(char) const override { return true; }
  };

  struct CharCondition : Condition {
    const char value;

    explicit CharCondition(char value) : value(value){};

    bool operator()(char ch) const override { return value == ch; }
  };

  struct CharSetCondition : Condition {
    const std::list<char> charList;
    bool isInverted = false;

    CharSetCondition(std::list<char> charList, bool isInverted)
        : charList(std::move(charList)), isInverted(isInverted){};

    bool operator()(char ch) const override {
      bool isWithinSet =
          std::find_if(charList.begin(), charList.end(), [&ch](char chToMatch) {
            return chToMatch == ch;
          }) != charList.end();
      return isInverted ^ isWithinSet;
    }
  };

  struct Token {
   public:
    virtual State &generate(Regex &regex, State &preState) const = 0;
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

  struct Char : Token {
   public:
    const char value;

    explicit Char(char value) : value(value){};

    State &generate(Regex &regex, State &preState) const override {
      State &newState = regex.createState();
      preState.addTransition(
          {std::make_unique<CharCondition>(value), &newState});
      return newState;
    };
  };

  struct Any : Token {
   public:
    State &generate(Regex &regex, State &preState) const override {
      State &newState = regex.createState();
      preState.addTransition({std::make_unique<AnyCondition>(), &newState});
      return newState;
    };
  };

  struct Group : Container {
   protected:
    std::list<std::unique_ptr<Token>> tokenList;

   public:
    State &generate(Regex &regex, State &preState) const override {
      State *currentState = &preState;
      for (const auto &token : tokenList) {
        currentState = &token->generate(regex, *currentState);
      }
      return *currentState;
    };

    bool push(std::unique_ptr<Token> token) override {
      tokenList.push_back(std::move(token));
      return false;
    };

    std::unique_ptr<Token> pop() override {
      auto temp = std::move(tokenList.back());
      tokenList.pop_back();
      return temp;
    };

    [[nodiscard]] size_t size() const override { return tokenList.size(); };
  };

  struct CharSet : Container {
   protected:
    std::list<Char> charList;

   public:
    bool isInverted = false;

    State &generate(Regex &regex, State &preState) const override {
      State &newState = regex.createState();
      std::list<char> charToMatchList;
      std::transform(charList.begin(), charList.end(), charToMatchList.begin(),
                     [](const Char &token) { return token.value; });
      preState.addTransition(
          {std::make_unique<CharSetCondition>(charToMatchList, isInverted),
           &newState});
      return newState;
    };

    bool push(std::unique_ptr<Token> token) override {
      auto *chToken = dynamic_cast<Char *>(token.get());
      if (chToken == nullptr)
        throw std::runtime_error(
            "Only single char is allowed within char class");
      charList.emplace_back(chToken->value);
      return false;
    };

    std::unique_ptr<Token> pop() override {
      throw std::runtime_error("Expected a character");
    };

    [[nodiscard]] size_t size() const override { return charList.size(); };
  };

  struct Alternation : Container {
   protected:
    const std::unique_ptr<Token> left;
    std::unique_ptr<Token> right;

   public:
    explicit Alternation(std::unique_ptr<Token> left,
                         std::unique_ptr<Token> right = nullptr)
        : left(std::move(left)), right(std::move(right)) {}

    State &generate(Regex &regex, State &preState) const override {
      State &newState = regex.createState();
      left->generate(regex, preState).addTransition({nullptr, &newState});
      right->generate(regex, preState).addTransition({nullptr, &newState});
      return newState;
    };

    bool push(std::unique_ptr<Token> token) override {
      right = std::move(token);
      return true;
    };

    std::unique_ptr<Token> pop() override {
      throw std::runtime_error("Expected a character or group");
    };

    [[nodiscard]] size_t size() const override {
      return right == nullptr ? 0 : 1;
    };
  };

  struct ZeroOrMore : Token {
   protected:
    std::unique_ptr<Token> token;

   public:
    explicit ZeroOrMore(std::unique_ptr<Token> token)
        : token(std::move(token)) {}

    State &generate(Regex &regex, State &preState) const override {
      State &finalState = token->generate(regex, preState);
      State &loopState = regex.createState();
      preState.addTransition({nullptr, &finalState});
      loopState.addTransition({nullptr, &preState});
      loopState.addTransition({nullptr, &finalState});
      return finalState;
    };
  };

  struct ZeroOrOnce : Token {
   protected:
    std::unique_ptr<Token> token;

   public:
    explicit ZeroOrOnce(std::unique_ptr<Token> token)
        : token(std::move(token)) {}

    State &generate(Regex &regex, State &preState) const override {
      State &finalState = token->generate(regex, preState);
      preState.addTransition({std::make_unique<AnyCondition>(), &finalState});
      return finalState;
    };
  };
};
}  // namespace GeneratedParser
