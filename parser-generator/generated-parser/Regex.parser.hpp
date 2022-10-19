#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <istream>
#include <iterator>
#include <list>
#include <memory>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include "Utility.parser.hpp"

namespace GeneratedParser {
class Regex {
 public:
  struct Token;
  struct Container;

 protected:
  using Stream = Utility::ForwardBufferedInputStream;

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
    void pushToTopContainer(size_t pos, std::unique_ptr<Container>& container) {
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

    [[nodiscard]] Container* top() const { return stack.top().get(); }
  };

  void parse(std::string_view regexStr) {
    ContainerStack stack;
    for (size_t pos = 0; pos < regexStr.size(); pos++) {
      char ch = regexStr.at(pos);
      switch (ch) {
        case '/':  // Delimiter
          if (pos == 0 || pos == regexStr.size() - 1)
            break;
          else if (pos == regexStr.size() - 2 && regexStr.at(++pos) == 'U') {
            _isGreedy = false;
            break;
          }
        default:
          stack.pushToTopContainer(pos, ch);
      }
    }
    if (stack.size() != 1)
      throw std::runtime_error("A group or char class is not closed");
    // Generate epsilon NFA
    stack.top()->generate(*this, createState());
  };

 public:
  struct Controller {
    [[nodiscard]] virtual char peek() = 0;
    virtual char get() = 0;
    virtual void consume() = 0;

    [[nodiscard]] virtual size_t record() const = 0;
    virtual void restore(size_t index) = 0;

    auto derive() { return DerivedController(*this); }
  };

  struct DerivedController : Controller {
   protected:
    Controller& controller;
    const size_t startPos;

   public:
    explicit DerivedController(Controller& controller)
        : controller(controller), startPos(controller.record()) {}

    ~DerivedController() { controller.restore(startPos); }

    [[nodiscard]] char peek() override { return controller.peek(); }

    char get() override { return controller.get(); }

    void consume() override { return controller.consume(); }

    [[nodiscard]] size_t record() const override { return controller.record(); }

    void restore(size_t index) override { return controller.restore(index); }

    auto derive() { return DerivedController(controller); }
  };

  struct StreamController : Controller {
   protected:
    Stream& stream;

   public:
    explicit StreamController(Stream& stream) : stream(stream){};

    [[nodiscard]] char peek() override {
      return static_cast<char>(stream.peek());
    }

    char get() override { return static_cast<char>(stream.get()); };

    void consume() override { stream.read(); }

    [[nodiscard]] size_t record() const override { return stream.tellg(); }

    void restore(size_t index) override { stream.seekg(index); }
  };

  struct StringController : Controller {
   protected:
    std::string_view str;
    size_t index = 0;

   public:
    explicit StringController(std::string_view str) : str(str){};

    [[nodiscard]] char peek() override {
      if (index >= str.size()) return EOF;
      return str.at(index);
    }

    char get() override {
      char currentChar = peek();
      index++;
      return currentChar;
    };

    void consume() override { index++; }

    [[nodiscard]] size_t record() const override { return index; }

    void restore(size_t index) override { this->index = index; }
  };

  struct Transition;
  struct State {
    friend struct Transition;

   protected:
    std::list<Transition> transitionList;

   public:
    void addTransition(Transition transition) {
      transitionList.push_back(std::move(transition));
    };

    void accept(DerivedController controller,
                std::unordered_set<const State*>& stateSet) const {
      for (const auto& transition : transitionList) {
        if (transition.condition) {
          if (transition.condition->operator()(controller.derive()))
            stateSet.insert(transition.state);
        } else {
          transition.state->accept(controller, stateSet);
        }
      }
    }

    [[nodiscard]] bool isMatched() const {
      if (transitionList.empty()) return true;
      return std::find_if(transitionList.begin(), transitionList.end(),
                          [](const Transition& transition) {
                            return transition.condition == nullptr &&
                                   transition.state->isMatched();
                          }) != transitionList.end();
    }
  };

  std::list<State> stateList;

 protected:
  bool _isGreedy = true;

  [[nodiscard]] State& createState() {
    stateList.emplace_back();
    return stateList.back();
  }

 public:
  explicit Regex(std::string_view regexStr) { parse(regexStr); };

  [[nodiscard]] const State& getStartState() const { return stateList.front(); }

  [[nodiscard]] const bool& isGreedy() const { return _isGreedy; }

  static bool isAnyStateMatch(
      std::unordered_set<const Regex::State*> currentState) {
    return std::find_if(currentState.begin(), currentState.end(),
                        [](const State* state) {
                          return state->isMatched();
                        }) != currentState.end();
  }

  [[nodiscard]] bool match(std::string_view str) const {
    StringController controller(str);
    return Regex::match(getStartState(), controller, _isGreedy);
  }

  [[nodiscard]] bool match(std::istream& stdStream) const {
    Stream stream(stdStream);
    return match(stream);
  }

  [[nodiscard]] bool match(Stream& stream) const {
    StreamController controller(stream);
    return Regex::match(getStartState(), controller, _isGreedy);
  }

  [[nodiscard]] static bool match(const State& startState,
                                  Controller& controller,
                                  bool isGreedy = true) {
    std::unordered_set<const Regex::State*> currentSet{&startState};
    std::unordered_set<const Regex::State*> nextSet{};
    int lastMatchedIndex = isGreedy && isAnyStateMatch(currentSet)
                               ? static_cast<int>(controller.record())
                               : -1;
    while (controller.peek() != EOF) {
      for (const auto& state : currentSet) {
        state->accept(controller.derive(), nextSet);
      }
      controller.consume();
      currentSet = nextSet;
      nextSet.clear();
      if (isGreedy) {
        if (currentSet.empty()) {
          if (lastMatchedIndex >= 0) {
            controller.restore(lastMatchedIndex);
            return true;
          }
          return false;
        }
        if (isAnyStateMatch(currentSet)) {
          lastMatchedIndex = static_cast<int>(controller.record());
        }
      } else {
        if (currentSet.empty()) return false;
        if (isAnyStateMatch(currentSet)) return true;
      }
    }
    return isGreedy ? isAnyStateMatch(currentSet) : false;
  }

  struct Condition {
   public:
    virtual bool operator()(DerivedController controller) const {
      return this->operator()(controller.get());
    };

   protected:
    virtual bool operator()(char) const { return false; };
  };

  struct Transition {
    std::unique_ptr<Condition> condition;
    const State* state;
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

  struct LookaheadCondition : Condition {
   public:
    const State* startState;
    const bool isInverted;

    LookaheadCondition(State* startState, bool isInverted)
        : startState(startState), isInverted(isInverted){};

    bool operator()(DerivedController controller) const override {
      return !isInverted & Regex::match(*startState, controller);
    }
  };

  struct CharRangeCondition : Condition {
   public:
    const char start;
    char end{};

    explicit CharRangeCondition(char start) : start(start){};
    CharRangeCondition(char start, char end) : start(start), end(end){};

    bool operator()(char ch) const override { return ch >= start && ch <= end; }
  };

  struct CharSetCondition : Condition {
   protected:
    std::list<std::shared_ptr<Condition>> conditionList;
    const bool isInverted;

   public:
    CharSetCondition(std::list<std::shared_ptr<Condition>> conditionList,
                     bool isInverted)
        : conditionList(std::move(conditionList)), isInverted(isInverted){};

    bool operator()(DerivedController controller) const override {
      size_t pos = controller.record();
      bool isInSet =
          std::find_if(
              conditionList.begin(), conditionList.end(),
              [&controller, &pos](const std::shared_ptr<Condition>& condition) {
                bool isMatched = condition->operator()(controller);
                controller.restore(pos);
                return isMatched;
              }) != conditionList.end();
      return isInverted ^ isInSet;
    }
  };

  struct Token {
   public:
    virtual State& generate(Regex& regex, State& preState) const = 0;
  };

  struct Container : Token {
   private:
    bool isEscaped = false;

   protected:
    bool isEscape(char ch) {
      if (ch == '\\') {
        isEscaped = true;
        return true;
      }
      return false;
    }

    bool tryEscape(char& ch) {
      bool oldIsEscaped = isEscaped;
      if (isEscaped) {
        isEscaped = false;
        switch (ch) {
          case 'n':
            ch = '\n';
            break;
        }
      }
      return oldIsEscaped;
    }

   public:
    /**
     * @return {bool}  : Whether should be finished immediately
     */
    virtual bool push(ContainerStack&, size_t, char) = 0;
    virtual bool push(ContainerStack&, size_t, std::unique_ptr<Container>&) = 0;

    virtual std::unique_ptr<Token> pop(size_t) = 0;

    [[nodiscard]] virtual size_t size() const = 0;
  };

  struct Char : Token {
   public:
    const char value;

    explicit Char(char value) : value(value){};

    State& generate(Regex& regex, State& preState) const override {
      State& endState = regex.createState();
      preState.addTransition(
          {std::make_unique<CharCondition>(value), &endState});
      return endState;
    };
  };

  struct Any : Token {
   public:
    State& generate(Regex& regex, State& preState) const override {
      State& endState = regex.createState();
      preState.addTransition({std::make_unique<AnyCondition>(), &endState});
      return endState;
    };
  };

  struct Group : Container {
   protected:
    std::list<std::unique_ptr<Token>> tokenList;

    bool isLookahead = false;
    bool isInverted = false;

   public:
    State& generate(Regex& regex, State& preState) const override {
      if (isLookahead) {
        State* currentState = &regex.createState();
        State* startState = currentState;
        for (const auto& token : tokenList) {
          currentState = &token->generate(regex, *currentState);
        }
        State& endState = regex.createState();
        preState.addTransition(
            {std::make_unique<LookaheadCondition>(startState, isInverted),
             &endState});
        return endState;
      }
      State* currentState = &preState;
      for (const auto& token : tokenList) {
        currentState = &token->generate(regex, *currentState);
      }
      return *currentState;
    };

    bool push(ContainerStack& stack, size_t pos, char ch) override {
      if (!tryEscape(ch)) {
        if (isEscape(ch)) return false;
        switch (ch) {
          case '(':
            stack.push(std::make_unique<Group>());
            return false;
          case ')':
            stack.finishTopContainer(pos);
            return false;
          case '[':
            stack.push(std::make_unique<CharSet>());
            return false;
          case '|':
            stack.push(std::make_unique<Alternation>(stack.popLastToken(pos)));
            return false;
          case '.':
            tokenList.push_back(std::make_unique<Any>());
            return false;
          case '*':
            tokenList.push_back(
                std::make_unique<ZeroOrMore>(stack.popLastToken(pos)));
            return false;
          case '?':
            if (tokenList.empty()) {
              isLookahead = true;
            } else
              tokenList.push_back(
                  std::make_unique<ZeroOrOnce>(stack.popLastToken(pos)));
            return false;
          case '=':
            if (isLookahead && tokenList.empty()) break;
          case '!':
            if (isLookahead && tokenList.empty()) {
              isInverted = true;
              return false;
            }
          case '+':
            tokenList.push_back(
                std::make_unique<OnceOrMore>(stack.popLastToken(pos)));
            return false;
          default:
            break;
        }
      }
      tokenList.push_back(std::make_unique<Char>(ch));
      return false;
    };

    bool push(ContainerStack&, size_t,
              std::unique_ptr<Container>& container) override {
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
    std::list<std::shared_ptr<Condition>> conditionList;

    CharRangeCondition* charRangeToBeFulfilled = nullptr;

   public:
    bool isInverted = false;

    State& generate(Regex& regex, State& preState) const override {
      State& endState = regex.createState();
      preState.addTransition(
          {std::make_unique<CharSetCondition>(conditionList, isInverted),
           &endState});
      return endState;
    };

    bool push(ContainerStack& stack, size_t pos, char ch) override {
      if (!tryEscape(ch)) {
        if (isEscape(ch)) return false;
        switch (ch) {
          case '-': {
            if (charRangeToBeFulfilled)
              throw std::runtime_error(
                  "Previous char range is not fulfilled: " +
                  std::to_string(pos));
            auto lastCondition = conditionList.back();
            conditionList.pop_back();
            const auto* start =
                dynamic_cast<const CharCondition*>(lastCondition.get());
            if (start == nullptr)
              throw std::runtime_error("Previous token must be a char: " +
                                       std::to_string(pos));
            auto charRangeCondition = std::make_unique<CharRangeCondition>(ch);
            charRangeToBeFulfilled = charRangeCondition.get();
            conditionList.push_back(std::move(charRangeCondition));
            return false;
          }
          case ']':
            stack.finishTopContainer(pos);
            return false;
          case '^':
            if (conditionList.empty()) {
              isInverted = true;
              return false;
            }
          default:
            break;
        }
      }
      if (charRangeToBeFulfilled) {
        charRangeToBeFulfilled->end = ch;
        charRangeToBeFulfilled = nullptr;
      } else
        conditionList.push_back(std::make_unique<CharCondition>(ch));
      return false;
    };

    bool push(ContainerStack&, size_t pos,
              std::unique_ptr<Container>&) override {
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

    State& generate(Regex& regex, State& preState) const override {
      State& endState = regex.createState();
      left->generate(regex, preState).addTransition({nullptr, &endState});
      right->generate(regex, preState).addTransition({nullptr, &endState});
      return endState;
    };

    bool push(ContainerStack& stack, size_t pos, char ch) override {
      if (!tryEscape(ch)) {
        if (isEscape(ch)) return false;
        switch (ch) {
          case '(':
            stack.push(std::make_unique<Group>());
            return false;
          case ')':
            stack.finishTopContainer(pos);
            return false;
          default:
            break;
        }
      }
      right = std::make_unique<Char>(ch);
      return true;
    };

    bool push(ContainerStack&, size_t,
              std::unique_ptr<Container>& container) override {
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

    State& generate(Regex& regex, State& preState) const override {
      State& endState = regex.createState();
      preState.addTransition({nullptr, &endState});
      State& tokenState = token->generate(regex, preState);
      tokenState.addTransition({nullptr, &preState});
      return endState;
    };
  };

  struct ZeroOrOnce : Token {
   protected:
    std::unique_ptr<Token> token;

   public:
    explicit ZeroOrOnce(std::unique_ptr<Token> token)
        : token(std::move(token)) {}

    State& generate(Regex& regex, State& preState) const override {
      State& endState = token->generate(regex, preState);
      preState.addTransition({nullptr, &endState});
      return endState;
    };
  };

  struct OnceOrMore : Token {
   protected:
    std::unique_ptr<Token> token;

   public:
    explicit OnceOrMore(std::unique_ptr<Token> token)
        : token(std::move(token)) {}

    State& generate(Regex& regex, State& preState) const override {
      State& endState = regex.createState();
      State& tokenState = token->generate(regex, preState);
      tokenState.addTransition({nullptr, &preState});
      tokenState.addTransition({nullptr, &endState});
      return endState;
    };
  };
};
}  // namespace GeneratedParser
