#pragma once

#include <istream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace GeneratedParser::Utility {
class ForwardBufferedInputStream {
 protected:
  std::istream& stream;
  std::vector<int> buffer;
  size_t index = 0;

 public:
  explicit ForwardBufferedInputStream(std::istream& stream) : stream(stream){};

  int peek() {
    while (index + 1 > buffer.size()) {
      if (!buffer.empty() && buffer.back() == EOF) return EOF;
      buffer.push_back(stream.get());
    }
    return buffer.at(index);
  }

  void read() { index++; }

  int get() {
    int currentChar = peek();
    index++;
    return currentChar;
  }

  [[nodiscard]] size_t tellg() const { return index; }

  void seekg(size_t index) { this->index = index; }

  void shrinkBufferToIndex() {
    buffer = {std::next(buffer.begin(), static_cast<int>(index)), buffer.end()};
    index = 0;
  }

  std::string getBufferToIndexAsString() {
    auto begin = buffer.begin();
    return {begin, std::next(begin, static_cast<int>(index))};
  }
};
}  // namespace GeneratedParser::Utility
