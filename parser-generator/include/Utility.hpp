#pragma once

#include <stack>
#include <string>

namespace ParserGenerator::Utility {
inline std::string escape(const std::string& str) {
  std::string str2;
  for (const auto& ch : str) {
    if (ch == '"' || ch == '\\') str2.push_back('\\');
    str2.push_back(ch);
  }
  return str2;
}

template <typename ItemType>
class IterableStack : public std::stack<ItemType> {
 public:
  IterableStack() : std::stack<ItemType>(){};

  explicit IterableStack(const std::deque<ItemType>& c)
      : std::stack<ItemType>(c) {}

  explicit IterableStack(std::deque<ItemType>&& c)
      : std::stack<ItemType>(std::move(c)) {}

  auto getContainer() { return this->c; }
};
}  // namespace ParserGenerator::Utility
