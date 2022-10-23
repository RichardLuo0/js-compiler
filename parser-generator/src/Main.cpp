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
#include "LLTablePasses.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Serializer.parser.hpp"

using namespace GeneratedParser::Serializer;

using TerminalType = ParserGenerator::TerminalType;
using BNFParser = ParserGenerator::BNFParser;
using BNFLexer = ParserGenerator::BNFLexer;

using LLTable = ParserGenerator::LLTable<size_t, size_t>;
using Production = LLTable::Production;
using Symbol = LLTable::Symbol;

using LLTablePasses = ParserGenerator::LLTablePasses<size_t, size_t>;

struct BuildInfo {
  friend BuildInfo transformToSizeTProductionList(
      const std::list<BNFParser::Production>& grammar);

 protected:
  std::list<Production> grammar;
  std::list<TerminalType> terminalList;
  std::unordered_map<std::string, size_t> nonTerminalIndexMap;

  std::unordered_map<size_t, std::list<size_t>> nonTerminalToExcludeCache;

 public:
  const std::list<size_t>& getDirectLeftCornerListOfNonTerminal(
      const std::string& nonTerminal) {
    size_t nonTerminalIndex = nonTerminalIndexMap.at(nonTerminal);
    if (!nonTerminalToExcludeCache.contains(nonTerminalIndex)) {
      auto& cache = nonTerminalToExcludeCache[nonTerminalIndex];
      for (auto p : grammar) {
        if (p.left == nonTerminalIndex && p.right.size() == 1 &&
            p.right.front().type == Symbol::Terminal) {
          cache.push_back(p.right.front().getTerminal());
        }
      }
    }
    return nonTerminalToExcludeCache.at(nonTerminalIndex);
  }

  std::list<Production>& getGrammar() { return grammar; }

  const std::list<TerminalType>& getTerminalList() { return terminalList; }

  const std::unordered_map<std::string, size_t>& getNonTerminalIndexMap() {
    return nonTerminalIndexMap;
  }
};

template <>
class GeneratedParser::Serializer::Serializer<BuildInfo> : public ISerializer {
 protected:
  BuildInfo& buildInfo;

 public:
  explicit Serializer(BuildInfo& buildInfo) : buildInfo(buildInfo) {}

  void serialize(BinaryOfStream& os) const override {
    const auto& terminalList = buildInfo.getTerminalList();
    Serializer<size_t>(terminalList.size()).serialize(os);
    for (const auto& item : terminalList) {
  void serialize(BinaryOfstream& os) const override {
    os.put(static_cast<BinaryOType>(buildInfo.terminalList.size()));
    for (const auto& item : buildInfo.terminalList) {
      os.put(item.type);
      switch (item.type) {
        case TerminalType::String:
        case TerminalType::Regex:
          Serializer<std::string>(item.value).serialize(os);
          break;
        case TerminalType::RegexExclude: {
          std::ranges::split_view terminalSplit(item.value, ' ');
          auto terminalSplitIt = terminalSplit.begin();
          auto regex = std::string_view{(*terminalSplitIt).begin(),
                                        (*terminalSplitIt).end()};
          Serializer<std::string_view>(regex).serialize(os);
          terminalSplitIt++;
          if (terminalSplitIt == terminalSplit.end())
            throw std::runtime_error("Not valid regex exclude expression");
          auto excludeNonTerminal =
              std::string{(*terminalSplitIt).begin(), (*terminalSplitIt).end()};
          Serializer<std::list<size_t>>(
              buildInfo.getDirectLeftCornerListOfNonTerminal(
                  excludeNonTerminal))
              .serialize(os);
          break;
        }
        default:
          throw std::runtime_error("Unknown terminal");
      }
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

  void serialize(BinaryOfStream& os) const override {
    os.put(symbol.type);
    switch (symbol.type) {
      case Symbol::Terminal:
        Serializer<size_t>(symbol.getTerminal()).serialize(os);
        break;
      case Symbol::NonTerminal:
        Serializer<size_t>(symbol.getNonTerminal()).serialize(os);
        break;
      default:
        break;
    }
  }
};

// Transform all LLTable<std::string, TerminalType>::Production to
// LLTable<size_t, size_t>::Production to reduce the memory cost and avoid
// string comparison
BuildInfo transformToSizeTProductionList(
    const std::list<BNFParser::Production>& grammar) {
  BuildInfo buildInfo;
  auto& [transformedGrammar, terminalList, nonTerminalIndexMap, _] = buildInfo;

  auto createNonTerminalIndex = [&](const std::string& nonTerminal) {
    if (nonTerminalIndexMap.contains(nonTerminal))
      return nonTerminalIndexMap.at(nonTerminal);
    size_t newLeft = nonTerminalIndexMap.size();
    nonTerminalIndexMap.emplace(nonTerminal, newLeft);
    return newLeft;
  };

  std::unordered_map<TerminalType, size_t> terminalIndexMap;
  for (const auto& production : grammar) {
    size_t newLeft = createNonTerminalIndex(production.left);

    std::list<Symbol> right;
    for (const auto& symbol : production.right) {
      if (symbol.type == BNFParser::Symbol::Terminal) {
        const auto& terminal = symbol.getTerminal();
        if (terminalIndexMap.contains(terminal))
          right.emplace_back(Symbol::Terminal, terminalIndexMap.at(terminal));
        else {
          size_t index = terminalList.size();
          right.emplace_back(Symbol::Terminal, index);
          terminalList.push_back(terminal);
          terminalIndexMap.emplace(terminal, index);
        }
      } else if (symbol.type == BNFParser::Symbol::NonTerminal)
        right.emplace_back(Symbol::NonTerminal,
                           createNonTerminalIndex(symbol.getNonTerminal()));
      else
        right.push_back(LLTable::END);
    }
    transformedGrammar.emplace_back(newLeft, right);
  }
  return buildInfo;
}

void outputToStream(const LLTable& table, BuildInfo& buildInfo,
                    BinaryOfStream& output) {
  BinarySerializer serializer;
  serializer.add(buildInfo);
  serializer.add(table.getStart());
  serializer.add(table.getTable());
  serializer.serialize(output);
}

void outputHeader(
    const std::unordered_map<std::string, size_t>& nonTerminalIndexMap,
    const std::string& fileName) {
  std::ofstream headerFile(fileName);
  headerFile << "namespace GeneratedParser {" << std::endl;
  for (const auto& [nonTerminal, index] : nonTerminalIndexMap) {
    headerFile << "constexpr inline size_t " << nonTerminal << " = " << index
               << ";" << std::endl;
  }
  headerFile << "}";
}

std::unordered_map<std::string, std::string> parseOption(
    int argc, const char** argv,
    std::unordered_map<std::string, std::string>&& defaultValue) {
  // Skip first argument
  argc--;
  argv++;
  std::unordered_map<std::string, std::string> options;
  std::string defaultArg = "default";
  std::string& currentSwitch = defaultArg;
  for (int i = 0; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.at(0) == '-') {
      currentSwitch = arg;
    } else {
      options[currentSwitch] = arg;
      currentSwitch = defaultArg;
    }
  }
  for (auto& [key, value] : defaultValue) {
    if (!options.contains(key)) options.emplace(key, value);
  }
  return options;
}

int main(int argc, const char** argv) {
  std::unordered_map<std::string, std::string> options =
      parseOption(argc, argv, {{"-o", "a.bin"}});

  if (!options.contains("default"))
    throw std::runtime_error("No bnf file is provided");

  std::ifstream bnfFile(options.at("default"));
  BNFParser parser(BNFLexer::create(bnfFile));

  BuildInfo buildInfo = transformToSizeTProductionList(parser.parse());

  size_t startIndex = buildInfo.getNonTerminalIndexMap().at("Start");
  size_t index = buildInfo.getNonTerminalIndexMap().size();
  LLTable table(startIndex, buildInfo.getGrammar(),
                [&](const size_t&) { return index++; });
  table.setFirstSetAnalysisPass<LLTablePasses::BuildFirstSetGraph>()
      .add<LLTablePasses::RemoveUnusedProduction>()
      .add<LLTablePasses::RemoveRightFirstEndProduction>()
      .add<LLTablePasses::EliminateLeftRecursion>()
      .add<LLTablePasses::EliminateBacktracking>()
      .build();

  std::string fileName = options.at("-o");
  BinaryOfStream of(fileName);
  outputToStream(table, buildInfo, of);

  if (options.contains("--header"))
    outputHeader(buildInfo.getNonTerminalIndexMap(), options.at("--header"));
}
