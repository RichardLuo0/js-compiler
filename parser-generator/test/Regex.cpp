#include "Regex.hpp"

#include <cassert>
#include <iostream>

using namespace GeneratedParser;

int test_Regex(int, char**) {
  Regex regex(R"("([^\\]|(\\"))*")");
  assert(!regex.match(R"("aa\"ab")"));
  return 0;
}
