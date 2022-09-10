#include "Regex.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>

using namespace ParserGenerator;

void Regex::State::addTransition(Transition transition) {
  transitionList.push_back(std::move(transition));
}

std::list<const Regex::State *> Regex::State::accept(char ch) const {
  std::list<const State *> stateList;
  for (const auto &transition : transitionList) {
    if (transition.condition) {
      if (transition.condition->operator()(ch))
        stateList.push_back(transition.state);
    } else {
      stateList.splice(stateList.end(), transition.state->accept(ch));
    }
  }
  return stateList;
}

bool Regex::State::isFinalState() const {
  if (transitionList.empty()) return true;
  return std::find_if(transitionList.begin(), transitionList.end(),
                      [](const Transition &transition) {
                        return transition.condition != nullptr ||
                               !transition.state->isFinalState();
                      }) == transitionList.end();
}

Regex::State &Regex::Char::generate(Regex &regex, State &preState) const {
  State &newState = regex.createState();
  preState.addTransition({std::make_unique<Char>(value), &newState});
  return newState;
}

bool Regex::Char::operator()(char ch) const { return value == ch; }

Regex::State &Regex::Any::generate(Regex &regex, State &preState) const {
  State &newState = regex.createState();
  preState.addTransition({std::make_unique<AnyCondition>(), &newState});
  return newState;
}

Regex::State &Regex::Group::generate(Regex &regex, State &preState) const {
  State *currentState = &preState;
  for (const auto &token : tokenList) {
    currentState = &token->generate(regex, *currentState);
  }
  return *currentState;
}

bool Regex::Group::push(std::unique_ptr<Token> token) {
  tokenList.push_back(std::move(token));
  return false;
}

std::unique_ptr<Regex::Token> Regex::Group::pop() {
  auto temp = std::move(tokenList.back());
  tokenList.pop_back();
  return temp;
}

size_t Regex::Group::size() const { return tokenList.size(); }

Regex::State &Regex::CharSet::generate(Regex &regex, State &preState) const {
  State &newState = regex.createState();
  preState.addTransition(
      {std::make_unique<CharSet>(charList, isInverted), &newState});
  return newState;
}

bool Regex::CharSet::push(std::unique_ptr<Token> token) {
  auto *chToken = dynamic_cast<Char *>(token.get());
  if (chToken == nullptr)
    throw std::runtime_error("Only single char is allowed within char class");
  charList.emplace_back(chToken->value);
  return false;
}

std::unique_ptr<Regex::Token> Regex::CharSet::pop() {
  throw std::runtime_error("Expected a character");
}

size_t Regex::CharSet::size() const { return charList.size(); }

bool Regex::CharSet::operator()(char ch) const {
  bool isWithinSet = std::find_if(charList.begin(), charList.end(),
                                  [&ch](const Char &chToken) {
                                    return chToken.value == ch;
                                  }) != charList.end();
  return isInverted ^ isWithinSet;
}

Regex::State &Regex::Alternation::generate(Regex &regex,
                                           State &preState) const {
  State &newState = regex.createState();
  left->generate(regex, preState).addTransition({nullptr, &newState});
  right->generate(regex, preState).addTransition({nullptr, &newState});
  return newState;
}

bool Regex::Alternation::push(std::unique_ptr<Token> token) {
  right = std::move(token);
  return true;
}

std::unique_ptr<Regex::Token> Regex::Alternation::pop() noexcept(false) {
  throw std::runtime_error("Expected a character or group");
}

size_t Regex::Alternation::size() const { return right == nullptr ? 0 : 1; };

Regex::State &Regex::ZeroOrMore::generate(Regex &regex, State &preState) const {
  State &finalState = token->generate(regex, preState);
  State &loopState = regex.createState();
  preState.addTransition({nullptr, &finalState});
  loopState.addTransition({nullptr, &preState});
  loopState.addTransition({nullptr, &finalState});
  return finalState;
}

Regex::State &Regex::ZeroOrOnce::generate(Regex &regex, State &preState) const {
  State &finalState = token->generate(regex, preState);
  preState.addTransition({std::make_unique<AnyCondition>(), &finalState});
  return finalState;
};

void Regex::parse(std::string regexStr) {
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
      case '?': {  // TODO non greedy
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
}

void Regex::build() {}

bool Regex::match(const std::string &str) {
  std::list<const Regex::State *> &&lastState{&stateList.front()};
  for (const auto &ch : str) {
    std::list<const Regex::State *> &&currentState{};
    for (auto &state : lastState) {
      currentState.splice(currentState.end(), state->accept(ch));
    }
    lastState = currentState;
  }
  return std::find_if(lastState.begin(), lastState.end(),
                      [](const State *state) {
                        return state->isFinalState();
                      }) != lastState.end();
}
