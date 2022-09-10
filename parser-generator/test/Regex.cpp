#include "Regex.hpp"

#include <cassert>
#include <iostream>

using namespace ParserGenerator;

int test_Regex(int, char**) {
  ParserGenerator::Regex regex(R"("([^\\]|(\\"))*")");
  assert(!regex.match(R"("aa\"ab")"));
  return 0;
}
