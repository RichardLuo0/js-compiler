#include <gtest/gtest.h>

#include <cassert>
#include <iostream>

#include "Regex.parser.hpp"

using namespace GeneratedParser;

TEST(Regex, String) {
  Regex regex(R"("([^\\]|(\\"))*")");
  EXPECT_TRUE(regex.match(R"("aa\"ab")"));
}
