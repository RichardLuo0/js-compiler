#include <corecrt.h>

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "CppGenerator.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Utils.hpp"

using namespace ParserGenerator;

inline std::string generateTerminal(
    const std::unordered_map<TerminalType, size_t>& reverseSearchMap,
    const S& symbol) {
  return std::to_string(reverseSearchMap.at(symbol.getTerminal()));
}

std::string generateTableString(
    const Table& table,
    const std::unordered_map<TerminalType, size_t>& reverseSearchMap) {
  std::string result = "{\n";
  for (auto [key, right] : table.getTable()) {
    result +=
        "{{\"" + Utils::escapeInPlace(key.first) + "\"," +
        (key.second.type == S::Terminal
             ? "S(" + generateTerminal(reverseSearchMap, key.second) + ")}"
             : "end}") +
        ",{";
    for (const S& symbol : right) {
      if (&symbol != &right.front()) result += ',';
      switch (symbol.type) {
        case S::Terminal:
          result += "S(" + generateTerminal(reverseSearchMap, symbol) + ")";
          break;
        case S::NonTerminal:
          result +=
              "S(\"" + Utils::escapeInPlace(symbol.getNonTerminal()) + "\")";
          break;
        case S::End:
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

void outputToStream(const Table& table, std::ostream& output) {
  std::unordered_map<TerminalType, size_t> reverseSearchMap;
  const auto& terminalList = table.getTerminalList();
  size_t index = 0;
  for (const auto& terminal : terminalList) {
    reverseSearchMap.emplace(terminal, index++);
  }

  CppFile file;
  file.addTopLevelExpression(
      std::make_unique<CppInclude>("GeneratedLexer.hpp"));
  file.addTopLevelExpression(
      std::make_unique<CppInclude>("GeneratedLLTable.hpp"));
  file.addTopLevelExpression(
      std::make_unique<CppInclude>("GeneratedParser.hpp"));
  file.addTopLevelExpression(std::make_unique<CppUsing>("GeneratedParser"));
  file.addTopLevelExpression(std::make_unique<CppMethod>(
      "Lexer::Lexer", std::list<std::string>{"std::istream& stream"},
      "stream(stream)", generateTerminalString(terminalList)));
  file.addTopLevelExpression(std::make_unique<CppMethod>(
      "GeneratedLLTable::GeneratedLLTable", std::list<std::string>{},
      "LLTable(\"" + table.getStart().getNonTerminal() + "\")",
      "table = " + generateTableString(table, reverseSearchMap) + ";\n"));
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
  std::unordered_map<std::string, int> subNonTerminalNameMap;
  Table table("Start", parser.parse(), [&](const std::string& nonTerminal) {
    subNonTerminalNameMap[nonTerminal] += 1;
    return nonTerminal + std::to_string(subNonTerminalNameMap[nonTerminal]);
  });

  if (options.count("-o")) {
    std::ofstream outputFile(options["-o"]);
    outputToStream(table, outputFile);
  } else {
    outputToStream(table, std::cout);
  }
}
