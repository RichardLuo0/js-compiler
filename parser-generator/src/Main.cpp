#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "CppGenerator.hpp"
#include "LLTable.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Utility.hpp"

using namespace ParserGenerator;

using Table = LLTable<std::string, size_t>;
using Production = Table::Production;
using Symbol = Table::Symbol;

struct TerminalListBuildInfo {
 public:
  const std::list<TerminalType>& terminalList;

 protected:
  const std::list<Production> productionList;
  std::unordered_map<std::string, std::vector<size_t>>
      nonTerminalToExcludeCache;

 public:
  explicit TerminalListBuildInfo(std::list<Production> productionList,
                                 const std::list<TerminalType>& terminalList)
      : terminalList(terminalList), productionList(std::move(productionList)){};

  std::vector<size_t> getOnlyDirectTerminalListFromNonTerminal(
      const std::string& nonTerminal) {
    if (!nonTerminalToExcludeCache.contains(nonTerminal)) {
      auto& cache = nonTerminalToExcludeCache[nonTerminal];
      for (auto p : productionList) {
        if (p.left == nonTerminal && p.right.size() == 1 &&
            p.right.front().type == Symbol::Terminal) {
          cache.push_back(p.right.front().getTerminal());
        }
      }
    }
    return nonTerminalToExcludeCache.at(nonTerminal);
  }
};

std::string buildTableString(const Table& table) {
  std::string result = "{\n";
  for (const auto& [nonTerminal, map] : table.getTable()) {
    result += "{\"" + Utility::escape(nonTerminal) + "\",\n{";
    for (const auto& [symbol, right] : map) {
      result +=
          "{" + (symbol.type == Symbol::Terminal
                     ? "Symbol(" + std::to_string(symbol.getTerminal()) + "),{"
                     : "end,{");
      for (const Symbol& symbol : right) {
        if (&symbol != &right.front()) result += ',';
        switch (symbol.type) {
          case Symbol::Terminal:
            result += "Symbol(" + std::to_string(symbol.getTerminal()) + ")";
            break;
          case Symbol::NonTerminal:
            result +=
                "Symbol(\"" + Utility::escape(symbol.getNonTerminal()) + "\")";
            break;
          case Symbol::End:
            result += "end";
          default:
            break;
        }
      }
      result += "}},\n";
    }
    result += "}},\n";
  }
  // Pop the last ,\n
  result.erase(result.size() - 2, 2);
  result += "\n}";
  return result;
}

std::string buildTerminalString(TerminalListBuildInfo& buildInfo) {
  const auto& terminalList = buildInfo.terminalList;
  std::string result;
  for (const auto& terminal : terminalList) {
    result += "matcherList.push_back(std::make_unique";
    switch (terminal.type) {
      case TerminalType::String:
        result += "<StringMatcher>(\"" + terminal.value + "\"";
        break;
      case TerminalType::Regex:
        result += "<RegexMatcher>(\"" + terminal.value + "\"";
        break;
      case TerminalType::RegexExclude: {
        auto terminalSplitIt =
            std::ranges::split_view(terminal.value, ' ').begin();
        auto regex =
            std::string((*terminalSplitIt).begin(), (*terminalSplitIt).end());
        terminalSplitIt++;
        auto excludeNonTerminal =
            std::string((*terminalSplitIt).begin(), (*terminalSplitIt).end());
        result +=
            "<RegexExcludeMatcher>(\"" + regex + "\",std::vector<size_t>{";
        for (size_t terminal :
             buildInfo.getOnlyDirectTerminalListFromNonTerminal(
                 excludeNonTerminal)) {
          result += std::to_string(terminal) + ",";
        }
        if (result.back() == ',') result.pop_back();
        result += "}";
        break;
      }
      default:
        throw std::runtime_error("Unknown terminal");
    }
    result += "));\n";
  }
  return result;
}

void outputToStream(const Table& table, TerminalListBuildInfo& buildInfo,
                    std::ostream& output) {
  CppFile file;
  file.addTopLevelExpression(std::make_unique<CppInclude>("Lexer.parser.hpp"));
  file.addTopLevelExpression(
      std::make_unique<CppInclude>("LLTable.parser.hpp"));
  file.addTopLevelExpression(std::make_unique<CppUsing>("GeneratedParser"));
  file.addTopLevelExpression(std::make_unique<CppMethod>(
      "Lexer::Lexer", std::list<std::string>{"std::istream& stream"},
      "stream(stream)", buildTerminalString(buildInfo)));
  file.addTopLevelExpression(std::make_unique<CppMethod>(
      "void", "GeneratedLLTable::generateTable", std::list<std::string>{},
      "table = " + buildTableString(table) + ";\n"));
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
    if (arg.at(0) == '-') {
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

  if (!options.contains("default"))
    throw std::runtime_error("No bnf file provided");

  std::ifstream bnfFile(options.at("default"));
  BNFParser parser(BNFLexer::create(bnfFile));

  // Convert TerminalType to size_t to avoid outputting very long string
  std::unordered_map<TerminalType, size_t> terminalMap;
  std::list<TerminalType> terminalList;
  std::list<Production> productionList;
  for (auto& production : parser.parse()) {
    std::list<Symbol> right;
    for (auto& symbol : production.right) {
      if (symbol.type == BNFParser::Symbol::Terminal) {
        const auto& terminal = symbol.getTerminal();
        if (!terminalMap.contains(terminal)) {
          size_t index = terminalList.size();
          right.emplace_back(index);
          terminalList.push_back(terminal);
          terminalMap[terminal] = index;
        } else
          right.emplace_back(terminalMap.at(terminal));
      } else if (symbol.type == BNFParser::Symbol::NonTerminal)
        right.emplace_back(symbol.getNonTerminal());
      else
        right.push_back(Table::end);
    }
    productionList.emplace_back(production.left, right);
  }
  TerminalListBuildInfo buildInfo(productionList, terminalList);

  std::unordered_map<std::string, size_t> subNonTerminalNameMap;
  Table table("Start", productionList, [&](const std::string& nonTerminal) {
    return nonTerminal + std::to_string(++subNonTerminalNameMap[nonTerminal]);
  });

  if (options.contains("-o")) {
    std::ofstream outputFile(options.at("-o"));
    outputToStream(table, buildInfo, outputFile);
  } else {
    outputToStream(table, buildInfo, std::cout);
  }
}
