#include "LLTable.hpp"

#include "Lexer.hpp"

using namespace LLTableGenerator;
using namespace JsCompiler;

using S = LLTable::Symbol;

LLTable::LLTable()
    : start(S("Start")),
      table({{{"B", S(Identifier)}, {S("b")}},
             {{"b", S(Identifier)}, {S(Identifier)}},
             {{"A", S(Add)}, {S("Start")}},
             {{"Start1", S(Identifier)}, {S("B")}},
             {{"Start1", S(Add)}, {S("A")}},
             {{"Start", S(Add)}, {S(Add), S("Start1")}}}) {}
