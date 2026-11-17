#ifndef LANG_VALUE_H
#define LANG_VALUE_H

#include "ast.h"
#include "error.h"
#include "token.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// 数组内部的洞与根变量的无值状态共用此标记，绝不以 0/空串代替。
struct Unassigned {};

struct Array;
using ArrayPtr = std::shared_ptr<Array>;

using Value = std::variant<Unassigned, std::int64_t, double, std::string, bool, ArrayPtr>;

// dimensions 表示从当前数组节点到叶元素的剩余维度，而非元素个数。
// shared_ptr 仅解决 C++ 递归类型存储；所有语言边界必须 deepCopy，禁止共享可变数组。
struct Array {
    std::size_t dimensions = 1;
    std::vector<Value> elements;
};

bool isUnassigned(const Value& value);
bool isInteger(const Value& value);
bool isFloat(const Value& value);
bool isNumeric(const Value& value);
bool isString(const Value& value);
bool isBoolean(const Value& value);
bool isArray(const Value& value);
bool isScalar(const Value& value);

std::int64_t asInteger(const Value& value);
double asFloat(const Value& value);
bool asBoolean(const Value& value);
const std::string& asString(const Value& value);
const Array& asArray(const Value& value);
Array& asArray(Value& value);

std::size_t valueDimensions(const Value& value);
Value makeArray(std::size_t dimensions);
Value deepCopy(const Value& value);

// 按语言规范估算值占用的内存（bit）：
//   标量 / 无值槽位 / Array 节点各 64 bit；字符串 64 + 字符数 × 16 bit；
//   数组为其节点开销加全部元素递归之和。溢出时饱和到 uint64 最大值。
std::uint64_t estimateValueBits(const Value& value);

// 把源标量转换为声明类型（manual 类型）。失败时抛出带位置的 LangError。
Value convertTo(const Value& value, TypeKind target, int line, int col);

// 二元运算：算术 / 比较 / 逻辑。失败时抛出 LangError。
Value applyBinary(TokenKind op, const Value& left, const Value& right, int line, int col);

// 一元运算：负号 / not。失败时抛出 LangError。
Value applyUnary(TokenKind op, const Value& operand, int line, int col);

// 标量保持原格式；数组递归打印，内部字符串带双引号，Unassigned 显示 unassigned。
std::string formatValue(const Value& value);

#endif
