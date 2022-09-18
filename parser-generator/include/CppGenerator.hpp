#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>

namespace ParserGenerator {
struct CppUnit {
  [[nodiscard]] virtual std::string output() const = 0;
};

struct CppTopLevelExpression : public CppUnit {};

struct CppInclude : public CppTopLevelExpression {
 protected:
  const std::string hppStr;

 public:
  explicit CppInclude(std::string hppStr) : hppStr(std::move(hppStr)){};

  [[nodiscard]] std::string output() const override {
    return "#include \"" + hppStr + "\"\n";
  }
};

struct CppUsing : public CppTopLevelExpression {
 protected:
  const std::string left;
  const std::string right;

 public:
  explicit CppUsing(std::string left) : left(std::move(left)) {}
  CppUsing(std::string left, std::string right)
      : left(std::move(left)), right(std::move(right)){};

  [[nodiscard]] std::string output() const override {
    if (right.empty()) return "using namespace " + left + ";\n";
    return "using " + left + " = " + right + ";\n";
  }
};

struct CppMethod final : public CppTopLevelExpression {
 protected:
  const std::string returnType;
  const std::string name;
  const std::list<std::string> args;
  const std::string initializer;
  const std::string body;

 public:
  // Constructor
  CppMethod(std::string name, std::list<std::string> args,
            std::string initializer = "", std::string body = "")
      : name(std::move(name)),
        args(std::move(args)),
        initializer(std::move(initializer)),
        body(std::move(body)) {}
  // Regular method
  CppMethod(std::string returnType, std::string name,
            std::list<std::string> args, std::string body = "")
      : returnType(std::move(returnType)),
        name(std::move(name)),
        args(std::move(args)),
        body(std::move(body)) {}

  [[nodiscard]] std::string output() const override {
    std::string result =
        (returnType.empty() ? "" : (returnType + " ")) + name + "(";
    std::for_each(args.begin(), args.end(),
                  [&result](const std::string& arg) { result += arg + ","; });
    if (!args.empty()) result.pop_back(); // Remove ','
    result += ")";
    if (!initializer.empty()) result += " : " + initializer;
    result += " {\n" + body + "};\n";
    return result;
  }
};

struct CppClass : public CppTopLevelExpression {
 protected:
  std::string name;
  std::list<CppMethod> methodList;

 public:
  void addMethod(const CppMethod& method) { methodList.push_back(method); }

  [[nodiscard]] std::string output() const override {
    std::string result = "class " + name + " {\n";
    std::for_each(
        methodList.begin(), methodList.end(),
        [&result](const CppMethod& method) { result += method.output(); });
    result += "}\n";
    return result;
  }
};

class CppFile : public CppUnit {
 protected:
  std::list<std::unique_ptr<CppTopLevelExpression>> topLevelList;

 public:
  void addTopLevelExpression(std::unique_ptr<CppTopLevelExpression> method) {
    topLevelList.push_back(std::move(method));
  }

  [[nodiscard]] std::string output() const override {
    std::string result = "// DO NOT EDIT; This file is auto-generated.\n";
    std::for_each(
        topLevelList.begin(), topLevelList.end(),
        [&result](const std::unique_ptr<CppTopLevelExpression>& method) {
          result += method->output();
        });
    return result;
  }
};
}  // namespace ParserGenerator
