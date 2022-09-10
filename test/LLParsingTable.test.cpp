#include "LLParsingTable.hpp"

#include <cassert>
#include <string>

enum TerminalType { LeftParen, RightParen, Plus, Multiply, Id };

template <>
struct std::hash<TerminalType> {
  size_t operator()(const TerminalType& terminal) const {
    return std::hash<int>{}(static_cast<int>(terminal));
  }
};

typedef JsCompiler::LLParsingTable<std::string, TerminalType> Table;
typedef Table::Production P;
typedef Table::Symbol S;

int test_LLParsingTable(int argc, char** argv) {
  Table table("E",
              {P("E", {S("T"), S("E1")}), P("E1", {S(Plus), S("T"), S("E1")}),
               P("E1", {Table::end}), P("T", {S("F"), S("T1")}),
               P("T1", {S(Multiply), S("F"), S("T1")}), P("T1", {Table::end}),
               P("F", {S(LeftParen), S("E"), S(RightParen)}), P("F", {S(Id)})});
  // auto&& output = table.parse({Id, Multiply, Id, Plus, Id});

  // std::vector<std::string> expectedOutput = {"E",  "T", "F", "T1", "F", "T1",
  //                                            "E1", "T", "F", "T1", "E1"};
  // size_t i = 0;
  // for (auto& production : output) {
  //   assert(production.getLeft() == expectedOutput[i]);
  //   i++;
  // }
  return 0;
}
