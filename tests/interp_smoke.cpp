#include "environment.h"
#include "error.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "source.h"
#include "value.h"

#include <cassert>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

struct RunResult {
    int exitCode = 0;
    std::string output;
    std::string error;
    std::string warnings;
    bool crashed = false;
};

RunResult run(const std::string& src, const std::string& input = "") {
    RunResult result;
    std::ostringstream out;
    std::ostringstream err;
    std::istringstream in(input);
    try {
        Source source{"<test>", src};
        Lexer lexer(source);
        Parser parser(source.file, lexer.tokenize());
        Program program = parser.parse();
        Interpreter interp("<test>", program, out, in, err);
        result.exitCode = interp.run();
    } catch (const LangError& error) {
        result.error = error.what();
        result.exitCode = 1;
    } catch (const std::exception& error) {
        result.error = error.what();
        result.crashed = true;
        result.exitCode = 2;
    } catch (...) {
        result.error = "unknown exception";
        result.crashed = true;
        result.exitCode = 2;
    }
    result.output = out.str();
    result.warnings = err.str();
    return result;
}

std::size_t countOccurrences(const std::string& text, const std::string& needle) {
    if (needle.empty()) {
        return 0;
    }
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

int passCount = 0;
int failCount = 0;

void check(const std::string& name, bool ok, const RunResult& r = RunResult{}) {
    if (ok) {
        ++passCount;
        std::cout << "[PASS] " << name << '\n';
    } else {
        ++failCount;
        std::cout << "[FAIL] " << name << '\n';
        std::cout << "    exit=" << r.exitCode
                  << " crashed=" << (r.crashed ? "yes" : "no") << '\n';
        std::cout << "    out=[" << r.output << "]\n";
        std::cout << "    err=[" << r.error << "]\n";
    }
    std::cout.flush();
}

int main() {
    std::cout << std::unitbuf;
    // 1. 基础输出与浮点格式化补 .0（output 无分隔符）
    {
        auto r = run("function main\n  output 1.0, 0.1, 1.1\n");
        check("float 格式化 1.0,0.1,1.1", r.exitCode == 0 && r.output == "1.00.11.1", r);
    }

    // 2. 整数除法返回 double
    {
        auto r = run("function main\n  output 1 / 2, 4.0 / 2.0\n");
        check("除法返回 double 0.5 与 2.0", r.exitCode == 0 && r.output == "0.52.0", r);
    }

    // 3. 变量状态机：free 后使用报错，reassign 可赋值
    {
        auto r = run("function main\n  initialize integer a 10\n  free a\n  output a\n");
        check("free 后使用 -> 错误", r.exitCode == 1 && r.error.find("尚未赋值") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a 10\n  free a\n  reassign a 2\n  output a\n");
        check("free 后 reassign 可赋值", r.exitCode == 0 && r.output == "2", r);
    }

    // 4. delete 后 reassign 报错；delete 未声明报错
    {
        auto r = run("function main\n  initialize integer a 10\n  delete a\n  reassign a 5\n");
        check("delete 后 reassign -> 错误", r.exitCode == 1 && r.error.find("未声明") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  delete ghost\n");
        check("delete 未声明 -> 错误", r.exitCode == 1 && r.error.find("未声明") != std::string::npos, r);
    }

    // 5. automatic 切换模式并保留值
    {
        auto r = run("function main\n  initialize integer a 10\n  automatic a\n  reassign a 5.5\n  output a\n");
        check("automatic 切换为 auto 并保留值", r.exitCode == 0 && r.output == "5.5", r);
    }
    {
        auto r = run("function main\n  automatic a\n");
        check("automatic 未声明变量 -> 错误", r.exitCode == 1 && r.error.find("未声明") != std::string::npos, r);
    }

    // 6. 类型转换：浮点->整数要求整数值；字符串->数值报错
    {
        auto r = run("function main\n  initialize integer a 1.5\n");
        check("float 1.5 -> integer 报错", r.exitCode == 1 && r.error.find("integer") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize float a \"x\"\n");
        check("string -> float 报错", r.exitCode == 1 && r.error.find("float") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a 5.0\n  output a\n");
        check("float 5.0 -> integer 合法", r.exitCode == 0 && r.output == "5", r);
    }

    // 7. 除零报错
    {
        auto r = run("function main\n  output 1 / 0\n");
        check("整数除零报错", r.exitCode == 1 && r.error.find("0") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  output 1.0 / 0.0\n");
        check("浮点除零报错", r.exitCode == 1, r);
    }

    // 8. 字符串与数值比较报错
    {
        auto r = run("function main\n  output \"a\" = 1\n");
        check("字符串与数值比较报错", r.exitCode == 1 && r.error.find("字符串") != std::string::npos, r);
    }

    // 9. not 优先级与比较链（解析期）
    {
        auto r = run("function main\n  if not true\n    output \"y\"\n  otherwise\n    output \"n\"\n");
        check("not true -> otherwise n", r.exitCode == 0 && r.output == "n", r);
    }
    {
        auto r = run("function main\n  output 1 < 2 < 3\n");
        check("比较链 1 < 2 < 3 解析报错", r.exitCode == 1 && r.error.find("链式") != std::string::npos, r);
    }

    // 10. if / otherwise
    {
        auto r = run("function main\n  if false\n    output \"y\"\n  otherwise\n    output \"n\"\n");
        check("if false -> n", r.exitCode == 0 && r.output == "n", r);
    }

    // 11. invoke 作为表达式（返回值）与语句（丢弃返回值）
    {
        auto r = run("function add a, b\n  return a + b\nfunction main\n  output invoke add 2, 3\n");
        check("invoke 表达式返回 5", r.exitCode == 0 && r.output == "5", r);
    }
    {
        auto r = run("function add a, b\n  return a + b\nfunction main\n  invoke add 2, 3\n  output \"y\"\n");
        check("invoke 语句丢弃返回值", r.exitCode == 0 && r.output == "y", r);
    }
    {
        auto r = run("function nothing\n  return\nfunction main\n  output invoke nothing\n");
        check("void 函数作表达式报错", r.exitCode == 1 && r.error.find("返回值") != std::string::npos, r);
    }

    // 12. exit 层级语义
    {
        auto r = run(
            "function main\n"
            "  initialize integer x 0\n"
            "  repeat\n"
            "    if x = 5\n"
            "      exit 2\n"
            "    reassign x x + 1\n"
            "  output \"done\"\n");
        check("exit 2 跳出 repeat+if 两级后继续", r.exitCode == 0 && r.output == "done", r);
    }
    {
        auto r = run(
            "function main\n"
            "  repeat\n"
            "    exit 1\n"
            "  output \"inside\"\n"
            "  output \"done\"\n");
        check("exit 1 跳出 repeat 后继续外层", r.exitCode == 0 && r.output == "insidedone", r);
    }
    {
        auto r = run(
            "function helper\n"
            "  exit 1\n"
            "function main\n"
            "  invoke helper\n"
            "  output \"after\"\n");
        check("exit N==层级 返回调用者", r.exitCode == 0 && r.output == "after", r);
    }
    {
        auto r = run(
            "function main\n"
            "  exit 5\n"
            "  output \"after\"\n");
        check("exit N>层级 退出程序", r.exitCode == 0 && r.output.empty(), r);
    }
    {
        auto r = run(
            "function main\n"
            "  exit 0\n"
            "  output \"continued\"\n");
        check("exit 0 继续执行", r.exitCode == 0 && r.output == "continued", r);
    }

    // 13. 嵌套函数返回与递归（实参分组用括号）
    {
        auto r = run(
            "function fib n\n"
            "  if n < 2\n"
            "    return n\n"
            "  return invoke fib (n - 1) + invoke fib (n - 2)\n"
            "function main\n"
            "  output invoke fib 10\n");
        check("递归 fib(10) = 55", r.exitCode == 0 && r.output == "55", r);
    }

    // 14. input：automatic 与 typed（变量必须先声明）
    {
        auto r = run("function main\n  initialize automatic a 0\n  input a\n  output a\n", "5");
        check("automatic input 整数", r.exitCode == 0 && r.output == "5", r);
    }
    {
        auto r = run("function main\n  initialize automatic a 0\n  input a\n  output a\n", "3.5");
        check("automatic input 浮点", r.exitCode == 0 && r.output == "3.5", r);
    }
    {
        auto r = run("function main\n  initialize automatic a 0\n  input a\n  output a\n", "hello");
        check("automatic input 字符串", r.exitCode == 0 && r.output == "hello", r);
    }
    {
        auto r = run("function main\n  initialize integer a 0\n  input a\n  output a\n", "42");
        check("typed integer input", r.exitCode == 0 && r.output == "42", r);
    }
    {
        auto r = run("function main\n  initialize integer a 0\n  input a\n  output a\n", "abc");
        check("typed integer input 非法 -> 错误", r.exitCode == 1, r);
    }
    {
        auto r = run("function main\n  initialize boolean b false\n  input b\n  output b\n", "true");
        check("typed boolean input true", r.exitCode == 0 && r.output == "true", r);
    }

    // 15. next / obtain：跨轮传值、首轮默认、自然结束清空、嵌套 repeat 隔离、索引校验
    {
        auto r = run(
            "function main\n"
            "  initialize integer i 0\n"
            "  repeat\n"
            "    reassign i i + 1\n"
            "    output obtain 1 : 0, \"|\", obtain 2 : \"d\", \"\\n\"\n"
            "    if i < 3\n"
            "      next i, \"v\"\n"
            "    if i = 4\n"
            "      exit 2\n");
        check("next/obtain 传值与自然结束清空", r.exitCode == 0 &&
              r.output == "0|d\n1|v\n2|v\n0|d\n", r);
    }
    {
        auto r = run(
            "function main\n"
            "  repeat\n"
            "    output obtain 3 : 42\n"
            "    exit 1\n");
        check("obtain 越界回退默认值", r.exitCode == 0 && r.output == "42", r);
    }
    {
        auto r = run(
            "function main\n"
            "  repeat\n"
            "    output obtain -1 : 0\n"
            "    exit 1\n");
        check("obtain 负索引 -> 错误", r.exitCode == 1 &&
              r.error.find("正整数") != std::string::npos, r);
    }
    {
        auto r = run(
            "function main\n"
            "  repeat\n"
            "    output obtain 1.5 : 0\n"
            "    exit 1\n");
        check("obtain 浮点索引 -> 错误", r.exitCode == 1 &&
              r.error.find("正整数") != std::string::npos, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer i 0\n"
            "  repeat\n"
            "    reassign i i + 1\n"
            "    repeat\n"
            "      output obtain 1 : \"in\", \"\\n\"\n"
            "      exit 1\n"
            "    if i = 2\n"
            "      exit 2\n"
            "    next \"out\"\n");
        check("嵌套 repeat 的 obtain 状态隔离", r.exitCode == 0 &&
              r.output == "in\nin\n", r);
    }
    {
        auto r = run("function main\n  next 1\n");
        check("next 在 repeat 外 -> 解析错误", r.exitCode == 1 &&
              r.error.find("repeat") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  output obtain 1 : 0\n");
        check("obtain 在 repeat 外 -> 解析错误", r.exitCode == 1 &&
              r.error.find("repeat") != std::string::npos, r);
    }

    // 16. 多维数组：空壳、部分下标、标量下标、类型/维度、深拷贝、input、状态快照
    {
        auto r = run(
            "function main\n"
            "  initialize integer g_4 []\n"
            "  reassign g_1_1_1_1 5\n"
            "  output g\n");
        check("四维空壳自动扩容", r.exitCode == 0 &&
              r.output == "[[[[5]]]]", r);
    }
    {
        auto r = run("function main\n  initialize float d_0 3.5\n  output d_0\n");
        check("标量 _0 下标 -> 错误", r.exitCode == 1 &&
              r.error.find("标量") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize float d_0 3.5\n  output d_1\n");
        check("标量 _1 下标 -> 错误", r.exitCode == 1 &&
              r.error.find("标量") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize float d_0 3.5\n  output d_0_1\n");
        check("标量 _0_1 下标 -> 错误", r.exitCode == 1 &&
              r.error.find("标量") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a_2 []\n  output a_1\n");
        check("部分下标返回空子数组", r.exitCode == 0 && r.output == "[]", r);
    }
    {
        auto r = run("function main\n  initialize integer a_2 []\n  output a_1_1_1\n");
        check("下标数超过声明维度 -> 错误", r.exitCode == 1 &&
              r.error.find("维度") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize automatic a [1, [2]]\n");
        check("不规则嵌套深度 -> 错误", r.exitCode == 1 &&
              r.error.find("维度") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize automatic a [1, \"x\", true]\n  output a\n");
        check("automatic 数组混合标量并带引号输出", r.exitCode == 0 &&
              r.output == "[1, \"x\", true]", r);
    }
    {
        auto r = run("function main\n  initialize integer a_2 []\n  reassign a_2_3 42\n  output a\n");
        check("稀疏扩容显示 unassigned", r.exitCode == 0 &&
              r.output == "[unassigned, [unassigned, unassigned, 42]]", r);
    }
    {
        auto r = run(
            "function helper p\n"
            "  reassign p_1 9\n"
            "  output p\n"
            "function main\n"
            "  initialize automatic a [1, 2]\n"
            "  invoke helper a\n"
            "  output a\n");
        check("数组函数参数深拷贝", r.exitCode == 0 && r.output == "[9, 2][1, 2]", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer i 0\n"
            "  repeat\n"
            "    reassign i i + 1\n"
            "    if i = 1\n"
            "      next [1, 2]\n"
            "    output obtain 1 : []\n"
            "    exit 1\n");
        check("next/obtain 数组快照", r.exitCode == 0 && r.output == "[1, 2]", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer a_1 []\n"
            "  input a_5\n"
            "  output a_5\n", "42");
        check("input 到数组叶元素", r.exitCode == 0 && r.output == "42", r);
    }
    {
        auto r = run("function main\n  initialize integer a_2 []\n  input a_1\n", "42");
        check("input 到子数组 -> 错误", r.exitCode == 1 &&
              r.error.find("类型不匹配") != std::string::npos, r);
    }

    // limit：内存估算、初始化降级、写入原子性、深拷贝与 input
    {
        auto r = run(
            "function main\n"
            "  initialize integer a_2 [] limit 192\n"
            "  reassign a_1_1 9\n"
            "  output a\n");
        check("limit 精确边界允许写入", r.exitCode == 0 && r.output == "[[9]]" &&
              r.warnings.empty(), r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer a_2 [] limit 200\n"
            "  reassign a_1_1 5\n"
            "  reassign a_10_10 7\n"
            "  reassign a_1 [1, 2, 3]\n"
            "  output a\n");
        check("limit 超限写入原子回滚且每次告警",
              r.exitCode == 0 && r.output == "[[5]]" &&
              countOccurrences(r.warnings, "超出内存限制 200 bit") == 2, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize string s \"abcdefgh\" limit 100\n"
            "  reassign s \"hi\"\n"
            "  output s\n");
        check("initialize 超限降级为无值且 limit 保留",
              r.exitCode == 0 && r.output == "hi" &&
              countOccurrences(r.warnings, "超出内存限制 100 bit") == 1, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer g_2 [[1], [2]] limit 200\n"
            "  output g, \"|\"\n"
            "  reassign g_1_1 7\n"
            "  output g\n");
        check("initialize 超限降级为维度空壳",
              r.exitCode == 0 && r.output == "[]|[[7]]" &&
              countOccurrences(r.warnings, "超出内存限制 200 bit") == 1, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize automatic a [1, 2, 3]\n"
            "  initialize automatic b [] limit 150\n"
            "  reassign b a\n"
            "  output b\n");
        check("深拷贝受 limit 约束",
              r.exitCode == 0 && r.output == "[]" &&
              countOccurrences(r.warnings, "超出内存限制 150 bit") == 1, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize string s \"\" limit 100\n"
            "  input s\n"
            "  output s\n"
            "  input s\n"
            "  output s\n",
            std::string(100, 'x') + "\nhi\n");
        check("input 超限跳过但消费文本流",
              r.exitCode == 0 && r.output == "hi" &&
              countOccurrences(r.warnings, "超出内存限制 100 bit") == 1, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize string s limit 96\n"
            "  reassign s \"你好\"\n"
            "  output s\n");
        check("字符串按 UTF-8 字符数估算",
              r.exitCode == 0 && r.output == "你好" && r.warnings.empty(), r);
    }
    {
        auto r = run("function main\n  initialize integer a 1 limit 0\n");
        check("limit 0 -> 错误", r.exitCode == 1 &&
              r.error.find("limit") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a 1 limit 2.5\n");
        check("limit 小数 -> 错误", r.exitCode == 1 &&
              r.error.find("limit") != std::string::npos, r);
    }

    // v0.5.0-rc.1：_size / _quantity 只读属性与 limit 下限
    {
        auto r = run(
            "function main\n"
            "  initialize automatic a [[[1, 2], [3, 4]], [[5, 6]], []]\n"
            "  output a_size, \" \", a_quantity\n");
        check("属性嵌套数组 size/quantity", r.exitCode == 0 && r.output == "832 2", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize automatic a [[1], []]\n"
            "  output a_size, \" \", a_quantity\n");
        check("quantity 统计非空子树", r.exitCode == 0 && r.output == "256 1", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize automatic a [[]]\n"
            "  output a_size, \" \", a_quantity\n");
        check("空数组 size/quantity", r.exitCode == 0 && r.output == "128 0", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer a\n"
            "  output a_size, \" \", a_quantity\n");
        check("无值根变量按 64bit/0 计", r.exitCode == 0 && r.output == "64 0", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize automatic a [1, 2]\n"
            "  output a_5_size, \" \", a_5_quantity, \" \", a_quantity, \" \", a_size\n");
        check("属性读取不扩容", r.exitCode == 0 && r.output == "64 0 2 192", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize automatic a [10, 20, 30]\n"
            "  initialize integer i 2\n"
            "  output a_i, \" \", a_(i), \" \", a_i_quantity\n");
        check("裸标识符下标与括号下标等价", r.exitCode == 0 && r.output == "20 20 1", r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize automatic a [1]\n"
            "  output a_1_2_size\n");
        check("属性下标数超过维度 -> 维度冲突", r.exitCode == 1 &&
              r.error.find("维度冲突") != std::string::npos, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer a 1\n"
            "  reassign a_size 1\n");
        check("属性目标只读", r.exitCode == 1 &&
              r.error.find("只读") != std::string::npos, r);
    }
    {
        auto r = run(
            "function main\n"
            "  initialize integer a 1\n"
            "  automatic a_quantity\n");
        check("automatic 属性目标 -> 只读", r.exitCode == 1 &&
              r.error.find("只读") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer size 1\n");
        check("size 关键字不能作标识符", r.exitCode == 1 &&
              r.error.find("关键字不能作为标识符") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a 1 limit 32\n");
        check("limit 低于槽位 -> 错误", r.exitCode == 1 &&
              r.error.find("limit 不能小于 64 bit") != std::string::npos, r);
    }
    {
        auto r = run("function main\n  initialize integer a 1 limit 0\n");
        check("limit 0 仍报正整数错误", r.exitCode == 1 &&
              r.error.find("limit 必须求值为正整数") != std::string::npos, r);
    }

    std::cout << "\n==== interp_smoke 汇总: PASS=" << passCount << " FAIL=" << failCount
              << " ====\n";
    std::cout.flush();
    return failCount == 0 ? 0 : 1;
}
