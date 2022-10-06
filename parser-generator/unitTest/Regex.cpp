#include <gtest/gtest.h>

#include "Regex.parser.hpp"

using namespace GeneratedParser;

TEST(Regex, String) {
  Regex regex(R"(/"([^\\]|(\\.))*"/)");
  EXPECT_TRUE(regex.match(R"("a\"b\c")"));
}
