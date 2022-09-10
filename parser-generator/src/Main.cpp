#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "Lexer.hpp"
#include "Parser.hpp"

using namespace ParserGenerator;

void printTable(Table table) {
  std::string result = "{";
  for (auto [key, production] : table.getTable()) {
    result +=
        "{{" + key.first + "," +
        (key.second.type == S::Terminal ? key.second.getTerminal().value + "}"
                                        : "}") +
        ",{";
    for (const S& symbol : production.getRight()) {
      switch (symbol.type) {
        case S::Terminal:
          result += "," + symbol.getTerminal().value;
          break;
        case S::NonTerminal:
          result += "," + symbol.getNonTerminal();
          break;
        case S::End:
          result += ",$";
        default:
          break;
      }
    }
    result += "}}\n";
  }
  result += "}\n";
  std::cout << result << std::endl;
}

void outputToFile(Table table, std::ofstream& output) {
  std::string result = "{";
  for (auto [key, production] : table.getTable()) {
    result +=
        "{{" + key.first + "," +
        (key.second.type == S::Terminal ? key.second.getTerminal().value + "}"
                                        : "}") +
        ",{";
    for (const S& symbol : production.getRight()) {
      switch (symbol.type) {
        case S::Terminal:
          result += "," + symbol.getTerminal().value;
          break;
        case S::NonTerminal:
          result += "," + symbol.getNonTerminal();
          break;
        case S::End:
          result += ",$";
        default:
          break;
      }
    }
    result += "}}\n";
  }
  result += "}\n";
  output << result << std::endl;
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
    outputToFile(table, outputFile);
  } else {
    printTable(table);
  }

  bnfFile.close();
}
