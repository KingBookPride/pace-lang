#ifndef LANG_INTERPRETER_H
#define LANG_INTERPRETER_H

#include "ast.h"
#include "environment.h"
#include "value.h"

#include <cstdint>
#include <iosfwd>
#include <iostream>
#include <string>
#include <vector>

// 控制流异常
struct ReturnSignal {
    bool hasValue = false;
    Value value{};
};

// 跳出 N 层 if/repeat 块后继续
struct ExitSignal {
    int levels = 0;
};

// 跳过当前 repeat 轮次的剩余语句，并把值传给下一轮
struct NextSignal {
    std::vector<Value> values;
};

// 立即退出整个程序（退出码 0），不跨越调用者
struct ProgramExit {};

class Interpreter {
public:
    Interpreter(
        std::string file,
        const Program& program,
        std::ostream& out = std::cout,
        std::istream& in = std::cin,
        std::ostream& err = std::cerr);

    // 执行 main 函数，返回进程退出码（0 或 1）
    int run();

private:
    Value eval(const Expr* expr);
    void execute(const Stmt* stmt);
    void executeBlock(const Block& body);     // if / otherwise 块
    void executeRepeat(const RepeatStmt& stmt);
    ReturnSignal callFunction(
        const std::string& name,
        std::vector<Value> arguments,
        int line,
        int col);

    Value parseTypedInput(const std::string& line, TypeKind type, int lline, int col);
    Value parseAutomaticInput(const std::string& line);

    // 只读属性：_size / _quantity，绝不触发自动扩容或写回。
    Value evalAttribute(const AttributeExpr& node);

    // limit：运行期求值并校验为自然数；超限只告警并跳过操作，绝不修改变量。
    std::uint64_t evalLimit(const Expr* expr);
    void warnOverLimit(const std::string& name, std::uint64_t limit);
    bool exceedsLimit(const Variable& variable, const Value& candidate) const;
    bool exceedsLimitBits(const Variable& variable, std::uint64_t bits) const;

    std::size_t evalArrayIndex(const Expr* expr);
    std::size_t evalDimensionExpression(const Expr* expr);
    std::vector<std::size_t> evalArrayIndices(
        const std::vector<std::unique_ptr<Expr>>& expressions);
    void validateValueForDimensions(
        Value& value,
        std::size_t dimensions,
        TypeKind type,
        int line,
        int col);
    void ensureArraySize(Array& array, std::size_t oneBased, int line, int col);

    // 只读访问：不物化任何路径，缺失的叶元素报无值，缺失的子数组返回空数组。
    Value readArrayValue(
        const Value& root,
        std::size_t dimensions,
        const std::vector<std::size_t>& indices,
        const std::string& name,
        int line,
        int col) const;
    bool readNeedsExpansion(
        const Variable& variable,
        const std::vector<std::size_t>& indices) const;
    void materializeReadPath(
        Value& root,
        std::size_t dimensions,
        const std::vector<std::size_t>& indices,
        int line,
        int col);
    void writeArrayValue(
        Value& root,
        std::size_t dimensions,
        const std::vector<std::size_t>& indices,
        const Value& value,
        int line,
        int col);
    Value readSubscript(
        Variable& variable,
        const std::string& name,
        const std::vector<std::unique_ptr<Expr>>& indices,
        int line,
        int col);
    void writeSubscript(
        Variable& variable,
        const std::string& name,
        const std::vector<std::size_t>& indices,
        Value value,
        int line,
        int col);

    std::string file_;
    Environment env_;
    std::ostream& out_;
    std::istream& in_;
    std::ostream& err_;
    int blockDepth_ = 0;
    // 每个处于执行中的 repeat 对应栈顶一项；obtain 读取最近一层循环的上一轮值。
    std::vector<const std::vector<Value>*> repeatValuesStack_;
};

#endif
