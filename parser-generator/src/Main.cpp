#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "CppGenerator.hpp"
#include "LLTable.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Utils.hpp"

using namespace ParserGenerator;

using Table = LLTable<std::string, size_t>;
using Production = Table::Production;
using Symbol = Table::Symbol;

std::string generateTableString(const Table& table) {
  std::string result = "{\n";
  for (auto [key, right] : table.getTable()) {
    result += "{{\"" + Utils::escapeInPlace(key.first) + "\"," +
              (key.second.type == Symbol::Terminal
                   ? "Symbol(" + std::to_string(key.second.getTerminal()) + ")}"
                   : "end}") +
              ",{";
    for (const Symbol& symbol : right) {
      if (&symbol != &right.front()) result += ',';
      switch (symbol.type) {
        case Symbol::Terminal:
          result += "Symbol(" + std::to_string(symbol.getTerminal()) + ")";
          break;
        case Symbol::NonTerminal:
          result += "Symbol(\"" +
                    Utils::escapeInPlace(symbol.getNonTerminal()) + "\")";
          break;
        case Symbol::End:
          result += "end";
        default:
          break;
      }
    }
    result += "}},\n";
  }
  // Pop the last ,\n
  result.erase(result.size() - 2, 2);
  result += "\n}";
  return result;
}

std::string generateTerminalString(
    const std::list<TerminalType>& terminalList) {
  std::string result;
  for (const auto& terminal : terminalList) {
    result += "matcherList.push_back(std::make_unique";
    switch (terminal.type) {
      case TerminalType::String:
        result += "<StringMatcher>(\"" + Utils::escapeInPlace(terminal.value);
        break;
      case TerminalType::Regex:
        result += "<RegexMatcher>(\"" + Utils::escapeInPlace(terminal.value);
        break;
      default:
        throw std::runtime_error("Unknown terminal");
    }
    result += "\"));\n";
  }
  return result;
}

void outputToStream(const Table& table,
                    const std::list<TerminalType>& terminalList,
                    std::ostream& output) {
  CppFile file;
  file.addTopLevelExpression(
      std::make_unique<CppInclude>("GeneratedLexer.hpp"));
  file.addTopLevelExpression(
      std::make_unique<CppInclude>("GeneratedLLTable.hpp"));
  file.addTopLevelExpression(std::make_unique<CppUsing>("GeneratedParser"));
  file.addTopLevelExpression(std::make_unique<CppMethod>(
      "Lexer::Lexer", std::list<std::string>{"std::istream& stream"},
      "stream(stream)", generateTerminalString(terminalList)));
  file.addTopLevelExpression(std::make_unique<CppMethod>(
      "GeneratedLLTable::GeneratedLLTable", std::list<std::string>{},
      "LLTable(\"" + table.getStart().getNonTerminal() + "\")",
      "table = " + generateTableString(table) + ";\n"));
  output << file.output();
}

std::unordered_map<std::string, std::string> parseOption(
    int argc, const char** argv,
    std::unordered_map<std::string, std::string>&& defaultValue) {
  std::unordered_map<std::string, std::string> options;
  std::string defaultArg = "default";
  std::string& currentSwitch = defaultArg;

  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if (arg[0] == '-') {
      currentSwitch = arg;
      continue;
    }
    options[currentSwitch] = arg;
    currentSwitch = defaultArg;
  }
  defaultValue.merge(options);
  return defaultValue;
}

int main(int argc, const char** argv) {
  std::unordered_map<std::string, std::string> options =
      parseOption(argc - 1, argv + 1, {});

  if (!options.count("default"))
    throw std::runtime_error("No bnf file provided");

  std::ifstream bnfFile(options["default"]);
  BNFParser parser(BNFLexer::create(bnfFile));

  // Convert terminal to size_t to avoid outputting very long string
  std::list<TerminalType> terminalList;
  std::list<Production> productionList;
  for (auto& production : parser.parse()) {
    std::list<Symbol> right;
    for (auto& symbol : production.right) {
      if (symbol.type == BNFParser::Symbol::Terminal) {
        const auto& terminal = symbol.getTerminal();
        right.emplace_back(terminalList.size());
        terminalList.push_back(terminal);
      } else if (symbol.type == BNFParser::Symbol::NonTerminal)
        right.emplace_back(symbol.getNonTerminal());
      else
        right.push_back(Table::end);
    }
    productionList.emplace_back(production.left, right);
  }

  std::unordered_map<std::string, int> subNonTerminalNameMap;
  Table table("Start", productionList, [&](const std::string& nonTerminal) {
    subNonTerminalNameMap[nonTerminal] += 1;
    return nonTerminal + std::to_string(subNonTerminalNameMap[nonTerminal]);
  });

  if (options.count("-o")) {
    std::ofstream outputFile(options["-o"]);
    outputToStream(table, terminalList, outputFile);
  } else {
    outputToStream(table, terminalList, std::cout);
  }
}
