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

#include "LLTable.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Serializer.parser.hpp"

using namespace ParserGenerator;
using namespace GeneratedParser::Serializer;

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

  const std::vector<size_t>& getOnlyDirectTerminalListFromNonTerminal(
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

template <>
class GeneratedParser::Serializer::Serializer<TerminalListBuildInfo>
    : public ISerializer {
 protected:
  TerminalListBuildInfo& buildInfo;

 public:
  explicit Serializer(TerminalListBuildInfo& buildInfo)
      : buildInfo(buildInfo) {}

  void serialize(BinaryOfstream& os) const override {
    os.put(buildInfo.terminalList.size());
    for (auto& item : buildInfo.terminalList) {
      os.put(item.type);
      switch (item.type) {
        case TerminalType::String:
        case TerminalType::Regex:
          Serializer<std::string>(item.value).serialize(os);
          break;
        case TerminalType::RegexExclude: {
          auto terminalSplitIt =
              std::ranges::split_view(item.value, ' ').begin();
          auto regex = std::string_view{(*terminalSplitIt).begin(),
                                        (*terminalSplitIt).end()};
          Serializer<std::string_view>(regex).serialize(os);
          terminalSplitIt++;
          auto excludeNonTerminal =
              std::string{(*terminalSplitIt).begin(), (*terminalSplitIt).end()};
          Serializer<std::vector<size_t>>(
              buildInfo.getOnlyDirectTerminalListFromNonTerminal(
                  excludeNonTerminal))
              .serialize(os);
          break;
        }
        default:
          throw std::runtime_error("Unknown terminal");
      }
      os.put(EOS);
    }
    os.put(EOS);
  }
};

template <>
class GeneratedParser::Serializer::Serializer<Symbol> : public ISerializer {
 protected:
  const Symbol& symbol;

 public:
  explicit Serializer(const Symbol& symbol) : symbol(symbol) {}

  void serialize(BinaryOfstream& os) const override {
    os.put(symbol.type);
    switch (symbol.type) {
      case Symbol::Terminal:
        Serializer<size_t>(symbol.getTerminal()).serialize(os);
        break;
      case Symbol::NonTerminal:
        Serializer<std::string>(symbol.getNonTerminal()).serialize(os);
        break;
      default:
        break;
    }
    os.put(EOS);
  }
};

void outputToStream(const Table& table, TerminalListBuildInfo& buildInfo,
                    BinaryOfstream& output) {
  BinarySerializer serializer;
  serializer.add(buildInfo);
  serializer.add(table.getTable());
  serializer.serialize(output);
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

  // Convert TerminalType to size_t to avoid outputting long string repeatedly
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
        right.push_back(Table::END);
    }
    productionList.emplace_back(production.left, right);
  }
  TerminalListBuildInfo buildInfo(productionList, terminalList);

  std::unordered_map<std::string, size_t> subNonTerminalNameMap;
  Table table("Start", productionList, [&](const std::string& nonTerminal) {
    return nonTerminal + "_" +
           std::to_string(++subNonTerminalNameMap[nonTerminal]);
  });

  std::string fileName = options.contains("-o") ? options.at("-o") : "a.bin";
  BinaryOfstream of(fileName);
  outputToStream(table, buildInfo, of);
}
