#ifndef LANG_INTERPRETER_H
#define LANG_INTERPRETER_H

#include "ast.h"
#include "environment.h"
#include "value.h"

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
        std::istream& in = std::cin);

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

    std::string file_;
    Environment env_;
    std::ostream& out_;
    std::istream& in_;
    int blockDepth_ = 0;
    // 每个处于执行中的 repeat 对应栈顶一项；obtain 读取最近一层循环的上一轮值。
    std::vector<const std::vector<Value>*> repeatValuesStack_;
};

#endif
