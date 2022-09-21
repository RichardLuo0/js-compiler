#pragma once

#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace GeneratedParser {
class Regex {
 public:
  struct Token;
  struct Container;

 protected:
  struct ContainerStack {
   protected:
    std::stack<std::unique_ptr<Container>> stack;

   public:
    ContainerStack() { stack.push(std::make_unique<Group>()); }

    std::unique_ptr<Token> popLastToken(size_t pos) {
      if (stack.top()->size() > 0) return stack.top()->pop(pos);
      auto topContainer = std::move(stack.top());
      stack.pop();
      return topContainer;
    }

    void pushToTopContainer(size_t pos, char ch) {
      if (stack.top()->push(*this, pos, ch)) finishTopContainer(pos);
    }
    void pushToTopContainer(size_t pos, std::unique_ptr<Container> &container) {
      if (stack.top()->push(*this, pos, container)) finishTopContainer(pos);
    }

    void finishTopContainer(size_t pos) {
      if (stack.size() < 2)
        throw std::runtime_error("No group to close: " + std::to_string(pos));
      std::unique_ptr<Container> current = std::move(stack.top());
      stack.pop();
      pushToTopContainer(pos, current);
    }

    void push(std::unique_ptr<Container> container) {
      stack.push(std::move(container));
    }

    [[nodiscard]] size_t size() const { return stack.size(); }

    [[nodiscard]] Container *top() const { return stack.top().get(); }
  };

  void parse(const std::string &regexStr) {
    ContainerStack stack;
    for (size_t i = 0; i < regexStr.size(); i++) {
      char ch = regexStr[i];
      switch (ch) {
        case '/':  // Delimiter
          if (i == regexStr.size() - 2) {
            if (regexStr[++i] == 'U') isGreedy = false;
          }
          break;
        case '\\':
          ch = regexStr[++i];
          switch (ch) {
            case 'n':
              ch = '\n';
              break;
          }
        default:
          stack.pushToTopContainer(i, ch);
      }
    }
    if (stack.size() != 1)
      throw std::runtime_error("A group or char class is not closed");
    // Generate epsilon NFA
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
      if (this->isFinalState())
        stateSet.insert(this);
      else
        for (const auto &transition : transitionList) {
          if (transition.condition) {
            if (transition.condition->operator()(ch))
              stateSet.insert(transition.state);
          } else {
            const auto &nextSet = transition.state->accept(ch);
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

  explicit Regex(const std::string &regexStr) { parse(regexStr); };

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
      auto nextSet = state->accept(ch);
      currentState.insert(nextSet.begin(), nextSet.end());
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

  [[nodiscard]] bool match(const std::string &str) const {
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
   public:
    const char value;

    explicit CharCondition(char value) : value(value){};

    bool operator()(char ch) const override { return value == ch; }
  };

  struct CharRangeCondition : Condition {
   public:
    const char start;
    char end{};

    explicit CharRangeCondition(char start) : start(start){};

    bool operator()(char ch) const override { return ch >= start && ch <= end; }
  };

  struct CharSetCondition : Condition {
   protected:
    std::list<std::shared_ptr<const Condition>> conditionList;
    const bool isInverted;

   public:
    CharSetCondition(std::list<std::shared_ptr<const Condition>> conditionList,
                     bool isInverted)
        : conditionList(std::move(conditionList)), isInverted(isInverted){};

    bool operator()(char ch) const override {
      bool isWithinSet =
          std::find_if(
              conditionList.begin(), conditionList.end(),
              [&ch](const std::shared_ptr<const Condition> &condition) {
                return condition->operator()(ch);
              }) != conditionList.end();
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
     * @return {bool}  : Should be finished after push or not.
     */
    virtual bool push(ContainerStack &, size_t, char) = 0;
    virtual bool push(ContainerStack &, size_t,
                      std::unique_ptr<Container> &) = 0;

    virtual std::unique_ptr<Token> pop(size_t) = 0;

    [[nodiscard]] virtual size_t size() const = 0;
  };

  struct Char : Token {
   public:
    const char value;

    explicit Char(char value) : value(value){};

    State &generate(Regex &regex, State &preState) const override {
      State &endState = regex.createState();
      preState.addTransition(
          {std::make_unique<CharCondition>(value), &endState});
      return endState;
    };
  };

  struct Any : Token {
   public:
    State &generate(Regex &regex, State &preState) const override {
      State &endState = regex.createState();
      preState.addTransition({std::make_unique<AnyCondition>(), &endState});
      return endState;
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

    bool push(ContainerStack &stack, size_t pos, char ch) override {
      switch (ch) {
        case '(':
          stack.push(std::make_unique<Group>());
          break;
        case ')':
          stack.finishTopContainer(pos);
          break;
        case '[':
          stack.push(std::make_unique<CharSet>());
          break;
        case '|':
          stack.push(std::make_unique<Alternation>(stack.popLastToken(pos)));
          break;
        case '.':
          tokenList.push_back(std::make_unique<Any>());
          break;
        case '*':
          tokenList.push_back(
              std::make_unique<ZeroOrMore>(stack.popLastToken(pos)));
          break;
        case '?':
          tokenList.push_back(
              std::make_unique<ZeroOrOnce>(stack.popLastToken(pos)));
          break;
        case '+':
          tokenList.push_back(
              std::make_unique<OnceOrMore>(stack.popLastToken(pos)));
          break;
        default:
          tokenList.push_back(std::make_unique<Char>(ch));
      }
      return false;
    };

    bool push(ContainerStack &, size_t,
              std::unique_ptr<Container> &container) override {
      tokenList.push_back(std::move(container));
      return false;
    }

    std::unique_ptr<Token> pop(size_t) override {
      auto temp = std::move(tokenList.back());
      tokenList.pop_back();
      return temp;
    };

    [[nodiscard]] size_t size() const override { return tokenList.size(); };
  };

  struct CharSet : Container {
   protected:
    std::list<std::shared_ptr<const Condition>> conditionList;

   public:
    bool isInverted = false;

    State &generate(Regex &regex, State &preState) const override {
      State &endState = regex.createState();
      preState.addTransition(
          {std::make_unique<CharSetCondition>(conditionList, isInverted),
           &endState});
      return endState;
    };

    bool push(ContainerStack &stack, size_t pos, char ch) override {
      CharRangeCondition *notFulfilledCharRangeCondition = nullptr;
      switch (ch) {
        case '-': {
          if (notFulfilledCharRangeCondition)
            throw std::runtime_error("Previous char range is not fulfilled: " +
                                     std::to_string(pos));
          const auto &lastCondition = conditionList.back();
          conditionList.pop_back();
          const auto *start =
              dynamic_cast<const CharCondition *>(lastCondition.get());
          if (start == nullptr)
            throw std::runtime_error("Previous token must be a char: " +
                                     std::to_string(pos));
          auto charRangeCondition = std::make_unique<CharRangeCondition>(ch);
          notFulfilledCharRangeCondition = charRangeCondition.get();
          conditionList.push_back(std::move(charRangeCondition));
          break;
        }
        case ']':
          stack.finishTopContainer(pos);
          break;
        case '^':
          if (conditionList.empty()) {
            isInverted = true;
            break;
          }
        default:
          if (notFulfilledCharRangeCondition) {
            notFulfilledCharRangeCondition->end = ch;
            notFulfilledCharRangeCondition = nullptr;
          } else
            conditionList.push_back(std::make_unique<CharCondition>(ch));
      }
      return false;
    };

    bool push(ContainerStack &, size_t pos,
              std::unique_ptr<Container> &) override {
      throw std::runtime_error("Charset does not allow container type :" +
                               std::to_string(pos));
    }

    std::unique_ptr<Token> pop(size_t pos) override {
      throw std::runtime_error("Expected a character: " + std::to_string(pos));
    };

    [[nodiscard]] size_t size() const override { return conditionList.size(); };
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
      State &endState = regex.createState();
      left->generate(regex, preState).addTransition({nullptr, &endState});
      right->generate(regex, preState).addTransition({nullptr, &endState});
      return endState;
    };

    bool push(ContainerStack &stack, size_t pos, char ch) override {
      switch (ch) {
        case '(':
          stack.push(std::make_unique<Group>());
          break;
        case ')':
          stack.finishTopContainer(pos);
          break;
        default:
          right = std::make_unique<Char>(ch);
          return true;
      }
      return false;
    };

    bool push(ContainerStack &, size_t,
              std::unique_ptr<Container> &container) override {
      right = std::move(container);
      return true;
    };

    std::unique_ptr<Token> pop(size_t pos) override {
      throw std::runtime_error("Expected a character or group: " +
                               std::to_string(pos));
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
      State &endState = regex.createState();
      preState.addTransition({nullptr, &endState});
      State &tokenState = token->generate(regex, preState);
      tokenState.addTransition({nullptr, &preState});
      tokenState.addTransition({nullptr, &endState});
      return endState;
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

  struct OnceOrMore : Token {
   protected:
    std::unique_ptr<Token> token;

   public:
    explicit OnceOrMore(std::unique_ptr<Token> token)
        : token(std::move(token)) {}

    State &generate(Regex &regex, State &preState) const override {
      State &endState = regex.createState();
      State &tokenState = token->generate(regex, preState);
      tokenState.addTransition({nullptr, &preState});
      tokenState.addTransition({nullptr, &endState});
      return endState;
    };
  };
};
}  // namespace GeneratedParser
