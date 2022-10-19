#pragma once

#include <stack>
#include <string>

namespace ParserGenerator::Utility {
template <typename ItemType>
class IterableStack : public std::stack<ItemType> {
 public:
  IterableStack() : std::stack<ItemType>(){};

  explicit IterableStack(const std::deque<ItemType>& c)
      : std::stack<ItemType>(c) {}

  explicit IterableStack(std::deque<ItemType>&& c)
      : std::stack<ItemType>(std::move(c)) {}

  const auto& getContainer() { return this->c; }
};
}  // namespace ParserGenerator::Utility
