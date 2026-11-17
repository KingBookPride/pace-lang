#include "interpreter.h"

#include "error.h"
#include "token.h"
#include "value.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return text.substr(begin, end - begin);
}

// 进入/离开 if/repeat 块时维护嵌套层数（异常路径亦正确）
struct BlockDepthGuard {
    explicit BlockDepthGuard(int* depth) : depth_(depth) { ++(*depth_); }
    ~BlockDepthGuard() { --(*depth_); }
    int* depth_;
};

// 函数帧的 RAII 守卫
struct FrameGuard {
    explicit FrameGuard(Environment* env) : env_(env) {}
    ~FrameGuard() { env_->popFrame(); }
    Environment* env_;
};

// repeat 值栈的 RAII 守卫：嵌套循环、exit/return 等异常路径均恢复外层状态
struct RepeatValuesGuard {
    RepeatValuesGuard(
        std::vector<const std::vector<Value>*>* stack,
        const std::vector<Value>* values)
        : stack_(stack) {
        stack_->push_back(values);
    }
    ~RepeatValuesGuard() { stack_->pop_back(); }
    std::vector<const std::vector<Value>*>* stack_;
};

std::string subscriptDisplay(
    const std::string& name,
    const std::vector<std::size_t>& indices) {
    std::string result = name;
    for (std::size_t index : indices) {
        result += "_" + std::to_string(index);
    }
    return result;
}

bool isIntegralDouble(double value) {
    return std::isfinite(value) && std::trunc(value) == value;
}

} // namespace

Interpreter::Interpreter(
    std::string file,
    const Program& program,
    std::ostream& out,
    std::istream& in,
    std::ostream& err)
    : file_(std::move(file)), out_(out), in_(in), err_(err) {
    for (const auto& function : program.functions) {
        env_.addFunction(function->name, function.get());
    }
}

int Interpreter::run() {
    try {
        const FunctionDef* mainFunction = env_.findFunction("main");
        if (mainFunction == nullptr) {
            // 解析期已保证存在，这里仅作防御
            throw LangError(file_, 1, 1, "程序缺少 main 函数");
        }
        (void)callFunction("main", {}, 0, 0);
        return 0;
    } catch (const ProgramExit&) {
        return 0;
    } catch (const ExitSignal&) {
        // 防御：不应到达顶层
        return 0;
    } catch (const std::bad_alloc&) {
        throw LangError(file_, 1, 1, "数组创建或复制时内存不足");
    } catch (const std::length_error&) {
        throw LangError(file_, 1, 1, "数组请求的大小超出解释器可表示范围");
    } catch (const LangError& error) {
        // 运行期错误在底层以空文件名抛出，这里统一补上源文件名，
        // 保证诊断格式始终为 文件:行:列: 错误: ...
        if (error.file.empty()) {
            throw LangError(file_, error.line, error.col, error.message);
        }
        throw;
    }
}

Value Interpreter::eval(const Expr* expr) {
    if (const auto* node = dynamic_cast<const IntLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const FloatLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const StrLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const BoolLit*>(expr)) {
        return node->value;
    }
    if (const auto* node = dynamic_cast<const ArrayLit*>(expr)) {
        Array array;
        array.elements.reserve(node->elements.size());
        std::size_t maxChildDimensions = 0;
        for (const auto& element : node->elements) {
            Value child = eval(element.get());
            maxChildDimensions = std::max(maxChildDimensions, valueDimensions(child));
            array.elements.push_back(deepCopy(child));
        }
        const std::size_t dimensions = 1 + maxChildDimensions;
        array.dimensions = dimensions;
        Value result = makeArray(dimensions);
        asArray(result) = std::move(array);
        validateValueForDimensions(
            result, dimensions, TypeKind::Automatic, node->line, node->col);
        return result;
    }
    if (const auto* node = dynamic_cast<const VarRef*>(expr)) {
        Variable* var = env_.find(node->name);
        if (var == nullptr) {
            throw LangError("", node->line, node->col, "变量 '" + node->name + "' 未声明");
        }
        if (!node->indices.empty()) {
            return readSubscript(*var, node->name, node->indices, node->line, node->col);
        }
        if (!var->hasValue) {
            throw LangError("", node->line, node->col,
                "变量 '" + node->name + "' 尚未赋值（处于无值状态）");
        }
        return deepCopy(var->value);
    }
    if (const auto* node = dynamic_cast<const ObtainExpr*>(expr)) {
        if (repeatValuesStack_.empty()) {
            // 解析期已拦截；保留运行期防御，避免手工构造 AST 时崩溃。
            throw LangError("", node->line, node->col, "obtain 只能在 repeat 内使用");
        }

        Value rawIndex = eval(node->index.get());
        if (!isInteger(rawIndex) || asInteger(rawIndex) <= 0) {
            throw LangError("", node->line, node->col,
                "obtain 索引必须是 1-based 正整数（不支持 0、负数或浮点数）");
        }
        const std::int64_t oneBased = asInteger(rawIndex);
        const std::vector<Value>& values = *repeatValuesStack_.back();
        if (static_cast<std::uint64_t>(oneBased) <=
            static_cast<std::uint64_t>(values.size())) {
            return deepCopy(values[static_cast<std::size_t>(oneBased - 1)]);
        }
        if (node->defaultValue) {
            return eval(node->defaultValue.get());
        }
        throw LangError("", node->line, node->col,
            "obtain 未取到上一轮 next 传入的对应值，且未提供默认值");
    }
    if (const auto* node = dynamic_cast<const Unary*>(expr)) {
        Value operand = eval(node->operand.get());
        return applyUnary(node->op, operand, node->line, node->col);
    }
    if (const auto* node = dynamic_cast<const Binary*>(expr)) {
        if (node->op == TokenKind::And) {
            Value left = eval(node->left.get());
            if (!isBoolean(left)) {
                throw LangError("", node->line, node->col, "and 要求布尔操作数");
            }
            if (!asBoolean(left)) return false; // 短路
            Value right = eval(node->right.get());
            if (!isBoolean(right)) {
                throw LangError("", node->line, node->col, "and 要求布尔操作数");
            }
            return asBoolean(right);
        }
        if (node->op == TokenKind::Or) {
            Value left = eval(node->left.get());
            if (!isBoolean(left)) {
                throw LangError("", node->line, node->col, "or 要求布尔操作数");
            }
            if (asBoolean(left)) return true; // 短路
            Value right = eval(node->right.get());
            if (!isBoolean(right)) {
                throw LangError("", node->line, node->col, "or 要求布尔操作数");
            }
            return asBoolean(right);
        }
        Value left = eval(node->left.get());
        Value right = eval(node->right.get());
        return applyBinary(node->op, left, right, node->line, node->col);
    }
    if (const auto* node = dynamic_cast<const InvokeExpr*>(expr)) {
        std::vector<Value> arguments;
        arguments.reserve(node->arguments.size());
        for (const auto& argument : node->arguments) {
            arguments.push_back(eval(argument.get()));
        }
        ReturnSignal result = callFunction(node->name, std::move(arguments), node->line, node->col);
        if (!result.hasValue) {
            throw LangError("", node->line, node->col,
                "函数 '" + node->name + "' 作为表达式调用时必须实际返回返回值");
        }
        return result.value;
    }
    throw LangError("", expr->line, expr->col, "无法求值的表达式");
}

std::size_t Interpreter::evalArrayIndex(const Expr* expr) {
    Value raw = eval(expr);
    if (isInteger(raw)) {
        const std::int64_t value = asInteger(raw);
        if (value <= 0) {
            throw LangError("", expr->line, expr->col,
                "数组下标必须求值为 1-based 正整数或整数值浮点数");
        }
        if (static_cast<std::uint64_t>(value) >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            throw LangError("", expr->line, expr->col, "数组下标过大，无法扩容");
        }
        return static_cast<std::size_t>(value);
    }
    if (isFloat(raw)) {
        const double value = asFloat(raw);
        const long double exact = static_cast<long double>(value);
        if (!isIntegralDouble(value) || value < 1.0 ||
            exact > static_cast<long double>(std::numeric_limits<std::size_t>::max())) {
            throw LangError("", expr->line, expr->col,
                "数组下标必须求值为 1-based 正整数或整数值浮点数");
        }
        return static_cast<std::size_t>(value);
    }
    throw LangError("", expr->line, expr->col,
        "数组下标必须求值为 1-based 正整数或整数值浮点数");
}

std::size_t Interpreter::evalDimensionExpression(const Expr* expr) {
    Value raw = eval(expr);
    if (isInteger(raw)) {
        const std::int64_t value = asInteger(raw);
        if (value < 0 ||
            static_cast<std::uint64_t>(value) >
                static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            throw LangError("", expr->line, expr->col,
                "维度声明必须求值为非负整数或整数值浮点数");
        }
        return static_cast<std::size_t>(value);
    }
    if (isFloat(raw)) {
        const double value = asFloat(raw);
        const long double exact = static_cast<long double>(value);
        if (!isIntegralDouble(value) || value < 0.0 ||
            exact > static_cast<long double>(std::numeric_limits<std::size_t>::max())) {
            throw LangError("", expr->line, expr->col,
                "维度声明必须求值为非负整数或整数值浮点数");
        }
        return static_cast<std::size_t>(value);
    }
    throw LangError("", expr->line, expr->col,
        "维度声明必须求值为非负整数或整数值浮点数");
}

std::vector<std::size_t> Interpreter::evalArrayIndices(
    const std::vector<std::unique_ptr<Expr>>& expressions) {
    std::vector<std::size_t> indices;
    indices.reserve(expressions.size());
    for (const auto& expression : expressions) {
        indices.push_back(evalArrayIndex(expression.get()));
    }
    return indices;
}

void Interpreter::validateValueForDimensions(
    Value& value,
    std::size_t dimensions,
    TypeKind type,
    int line,
    int col) {
    if (dimensions == 0) {
        if (isArray(value) || isUnassigned(value)) {
            throw LangError("", line, col,
                "维度冲突：叶元素必须是标量，不能是数组或无值状态");
        }
        if (type != TypeKind::Automatic) {
            value = convertTo(value, type, line, col);
        }
        return;
    }

    if (!isArray(value)) {
        throw LangError("", line, col,
            "维度冲突：目标需要 " + std::to_string(dimensions) +
                " 维数组，但得到的是标量");
    }
    Array& array = asArray(value);
    if (array.dimensions != dimensions) {
        if (!array.elements.empty()) {
            throw LangError("", line, col,
                "维度冲突：数组声明维度与实际嵌套维度不一致");
        }
        // 空壳 [] 可适配任意剩余维度。
        array.dimensions = dimensions;
    }

    for (Value& element : array.elements) {
        if (isUnassigned(element)) {
            continue;
        }
        if (dimensions == 1) {
            if (isArray(element)) {
                throw LangError("", line, col,
                    "维度冲突：一维数组的叶元素不能是数组");
            }
            if (type != TypeKind::Automatic) {
                element = convertTo(element, type, line, col);
            }
        } else {
            if (!isArray(element)) {
                throw LangError("", line, col,
                    "维度冲突：同一数组的非空分支嵌套深度必须一致");
            }
            validateValueForDimensions(element, dimensions - 1, type, line, col);
        }
    }
}

void Interpreter::ensureArraySize(
    Array& array,
    std::size_t oneBased,
    int line,
    int col) {
    if (oneBased > array.elements.max_size()) {
        throw LangError("", line, col, "数组下标过大，无法扩容");
    }
    if (oneBased <= array.elements.size()) {
        return;
    }
    try {
        array.elements.resize(oneBased, Value{Unassigned{}});
    } catch (const std::bad_alloc&) {
        throw LangError("", line, col, "数组扩容内存不足");
    } catch (const std::length_error&) {
        throw LangError("", line, col, "数组下标过大，无法扩容");
    }
}

std::uint64_t Interpreter::evalLimit(const Expr* expr) {
    const Value raw = eval(expr);
    if (isInteger(raw)) {
        const std::int64_t value = asInteger(raw);
        if (value <= 0) {
            throw LangError("", expr->line, expr->col,
                "limit 必须求值为正整数（或小数部分为 0 的正浮点数）");
        }
        return static_cast<std::uint64_t>(value);
    }
    if (isFloat(raw)) {
        const double value = asFloat(raw);
        const long double exact = static_cast<long double>(value);
        if (!isIntegralDouble(value) || value < 1.0 ||
            exact > static_cast<long double>(std::numeric_limits<std::uint64_t>::max())) {
            throw LangError("", expr->line, expr->col,
                "limit 必须求值为正整数（或小数部分为 0 的正浮点数）");
        }
        return static_cast<std::uint64_t>(value);
    }
    throw LangError("", expr->line, expr->col,
        "limit 必须求值为正整数（或小数部分为 0 的正浮点数）");
}

void Interpreter::warnOverLimit(const std::string& name, std::uint64_t limit) {
    // 规范要求的固定格式；不做去重，每次触发都打印一行。
    err_ << "Warning: 变量 '" << name << "' 超出内存限制 " << limit
         << " bit，操作被跳过。" << '\n';
}

bool Interpreter::exceedsLimitBits(
    const Variable& variable,
    std::uint64_t bits) const {
    return variable.limitBits != 0 && bits > variable.limitBits;
}

bool Interpreter::exceedsLimit(
    const Variable& variable,
    const Value& candidate) const {
    return exceedsLimitBits(variable, estimateValueBits(candidate));
}

// 只读访问：不物化任何路径。缺失的完整叶元素报无值；缺失的部分下标返回空子数组。
Value Interpreter::readArrayValue(
    const Value& root,
    std::size_t dimensions,
    const std::vector<std::size_t>& indices,
    const std::string& name,
    int line,
    int col) const {
    const Array* current = &asArray(root);
    const bool fullLeafRead = indices.size() == dimensions;

    for (std::size_t depth = 0; depth < indices.size(); ++depth) {
        const std::size_t index = indices[depth];
        const bool isLast = depth + 1 == indices.size();

        if (index > current->elements.size()) {
            if (fullLeafRead) {
                throw LangError("", line, col,
                    "变量 '" + subscriptDisplay(name, indices) +
                        "' 处于无值状态，无法使用");
            }
            return makeArray(dimensions - indices.size());
        }

        const Value& slot = current->elements[index - 1];
        if (isLast) {
            if (!fullLeafRead) {
                if (isUnassigned(slot)) {
                    return makeArray(dimensions - indices.size());
                }
                if (!isArray(slot)) {
                    throw LangError("", line, col,
                        "维度冲突：部分下标必须引用子数组");
                }
                return deepCopy(slot);
            }
            if (isUnassigned(slot)) {
                throw LangError("", line, col,
                    "变量 '" + subscriptDisplay(name, indices) +
                        "' 处于无值状态，无法使用");
            }
            if (isArray(slot)) {
                throw LangError("", line, col,
                    "维度冲突：完整下标的叶元素不能是数组");
            }
            return deepCopy(slot);
        }

        if (isUnassigned(slot)) {
            if (fullLeafRead) {
                throw LangError("", line, col,
                    "变量 '" + subscriptDisplay(name, indices) +
                        "' 处于无值状态，无法使用");
            }
            return makeArray(dimensions - indices.size());
        }
        if (!isArray(slot)) {
            throw LangError("", line, col,
                "维度冲突：数组中间层必须是数组或无值状态");
        }
        current = &asArray(slot);
    }
    throw LangError("", line, col, "内部错误：下标访问未到达目标");
}

bool Interpreter::readNeedsExpansion(
    const Variable& variable,
    const std::vector<std::size_t>& indices) const {
    const Array* current = &asArray(variable.value);
    for (std::size_t depth = 0; depth < indices.size(); ++depth) {
        const std::size_t index = indices[depth];
        if (index > current->elements.size()) {
            return true;
        }
        const bool isLast = depth + 1 == indices.size();
        const Value& slot = current->elements[index - 1];
        if (isLast) {
            return indices.size() < variable.dimensions && isUnassigned(slot);
        }
        if (isUnassigned(slot)) {
            return true;
        }
        if (!isArray(slot)) {
            return false; // 结构错误交给 readArrayValue 统一报维度冲突
        }
        current = &asArray(slot);
    }
    return false;
}

void Interpreter::materializeReadPath(
    Value& root,
    std::size_t dimensions,
    const std::vector<std::size_t>& indices,
    int line,
    int col) {
    Array* current = &asArray(root);
    if (current->dimensions != dimensions) {
        if (!current->elements.empty()) {
            throw LangError("", line, col,
                "维度冲突：数组根节点维度不一致");
        }
        current->dimensions = dimensions;
    }

    for (std::size_t depth = 0; depth < indices.size(); ++depth) {
        ensureArraySize(*current, indices[depth], line, col);
        Value& slot = current->elements[indices[depth] - 1];
        const std::size_t remaining = dimensions - depth - 1;
        const bool isLast = depth + 1 == indices.size();

        if (isUnassigned(slot)) {
            if (isLast && indices.size() == dimensions) {
                return; // 完整叶元素保持无值，随后只读访问会报无值错误
            }
            slot = makeArray(remaining);
        }
        if (isLast) {
            // 部分下标引用子数组；若为标量，由只读访问统一报维度冲突。
            return;
        }
        if (!isArray(slot)) {
            throw LangError("", line, col,
                "维度冲突：数组中间层必须是数组或无值状态");
        }
        current = &asArray(slot);
    }
}

Value Interpreter::readSubscript(
    Variable& variable,
    const std::string& name,
    const std::vector<std::unique_ptr<Expr>>& indexExpressions,
    int line,
    int col) {
    if (variable.dimensions == 0) {
        throw LangError("", line, col,
            "维度冲突：0 维标量不能使用下标访问");
    }
    if (indexExpressions.size() > variable.dimensions) {
        throw LangError("", line, col,
            "维度冲突：下标数量超过变量的声明维度");
    }
    if (!variable.hasValue) {
        throw LangError("", line, col,
            "变量尚未赋值（处于无值状态），不能使用下标访问");
    }
    if (!isArray(variable.value)) {
        throw LangError("", line, col, "维度冲突：变量内部不是数组");
    }

    const std::vector<std::size_t> indices = evalArrayIndices(indexExpressions);
    if (!readNeedsExpansion(variable, indices)) {
        return readArrayValue(variable.value, variable.dimensions, indices, name, line, col);
    }

    // 扩容也在副本上完成：超限或异常时变量保持原状，绝不留半建成的空壳。
    Value working = deepCopy(variable.value);
    materializeReadPath(working, variable.dimensions, indices, line, col);
    if (exceedsLimit(variable, working)) {
        warnOverLimit(name, variable.limitBits);
        return readArrayValue(variable.value, variable.dimensions, indices, name, line, col);
    }

    Value result = readArrayValue(working, variable.dimensions, indices, name, line, col);
    variable.value = std::move(working);
    return result;
}

void Interpreter::writeArrayValue(
    Value& root,
    std::size_t dimensions,
    const std::vector<std::size_t>& indices,
    const Value& value,
    int line,
    int col) {
    Array* current = &asArray(root);
    if (current->dimensions != dimensions) {
        if (!current->elements.empty()) {
            throw LangError("", line, col,
                "维度冲突：数组根节点维度不一致");
        }
        current->dimensions = dimensions;
    }

    for (std::size_t depth = 0; depth < indices.size(); ++depth) {
        ensureArraySize(*current, indices[depth], line, col);
        Value& slot = current->elements[indices[depth] - 1];
        if (depth + 1 == indices.size()) {
            slot = deepCopy(value);
            return;
        }

        const std::size_t remaining = dimensions - depth - 1;
        if (isUnassigned(slot)) {
            slot = makeArray(remaining);
        }
        if (!isArray(slot)) {
            throw LangError("", line, col,
                "维度冲突：数组中间层必须是数组或无值状态");
        }
        validateValueForDimensions(
            slot, remaining, TypeKind::Automatic, line, col);
        current = &asArray(slot);
    }
}

void Interpreter::writeSubscript(
    Variable& variable,
    const std::string& name,
    const std::vector<std::size_t>& indices,
    Value value,
    int line,
    int col) {
    if (variable.dimensions == 0) {
        throw LangError("", line, col,
            "维度冲突：0 维标量不能使用下标赋值");
    }
    if (indices.empty()) {
        throw LangError("", line, col, "内部错误：writeSubscript 需要至少一个下标");
    }
    if (indices.size() > variable.dimensions) {
        throw LangError("", line, col,
            "维度冲突：下标数量超过变量的声明维度");
    }
    if (!variable.hasValue) {
        throw LangError("", line, col,
            "变量处于无值状态，不能使用下标赋值；请先整体 reassign");
    }
    if (!isArray(variable.value)) {
        throw LangError("", line, col, "维度冲突：变量内部不是数组");
    }

    const std::size_t remainingDimensions = variable.dimensions - indices.size();
    validateValueForDimensions(
        value, remainingDimensions, variable.declaredType, line, col);

    // 全部修改先落在副本上；只有内存估算通过后才原子替换。
    Value working = deepCopy(variable.value);
    writeArrayValue(working, variable.dimensions, indices, value, line, col);
    if (exceedsLimit(variable, working)) {
        warnOverLimit(name, variable.limitBits);
        return;
    }

    variable.value = std::move(working);
    variable.hasValue = true;
}


void Interpreter::execute(const Stmt* stmt) {
    if (const auto* node = dynamic_cast<const InitializeStmt*>(stmt)) {
        for (const auto& decl : node->declarations) {
            std::size_t dimensions = 0;
            if (decl.dimension) {
                dimensions = evalDimensionExpression(decl.dimension.get());
            }

            const std::uint64_t limitBits =
                decl.limit ? evalLimit(decl.limit.get()) : 0;

            Value raw;
            bool hasValue = false;
            if (decl.value) {
                raw = eval(decl.value.get());
                hasValue = true;
            } else if (decl.dimension && dimensions > 0) {
                raw = makeArray(dimensions);
                hasValue = true;
            }

            if (!decl.dimension) {
                dimensions = decl.value ? valueDimensions(raw) : 0;
            }

            if (dimensions == 0 && !decl.value) {
                // 无初值标量：已声明但无值，占用 0 bit，必然满足 limit。
                env_.declare(
                    decl.name, decl.type, false, Value{Unassigned{}}, 0, limitBits);
                continue;
            }
            if (dimensions > 0) {
                // 显式 N 维空壳与数组初值都进入统一结构校验。
                hasValue = true;
            }

            validateValueForDimensions(
                raw, dimensions, decl.type, decl.line, decl.col);

            const std::uint64_t postBits = hasValue ? estimateValueBits(raw) : 0;
            if (limitBits != 0 && postBits > limitBits) {
                // 初始化超限降级：不赋予初值；能容纳空壳时保留维度空壳，
                // 否则退化为无值状态，保证变量占用始终不超过 limit。
                warnOverLimit(decl.name, limitBits);
                if (dimensions > 0 && limitBits >= 64) {
                    env_.declare(
                        decl.name,
                        decl.type,
                        true,
                        makeArray(dimensions),
                        dimensions,
                        limitBits);
                } else {
                    env_.declare(
                        decl.name,
                        decl.type,
                        false,
                        Value{Unassigned{}},
                        dimensions,
                        limitBits);
                }
                continue;
            }

            env_.declare(decl.name, decl.type, hasValue, raw, dimensions, limitBits);
        }
        return;
    }
    if (const auto* node = dynamic_cast<const ReassignStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);
        if (node->indices.empty()) {
            Value raw = eval(node->value.get());
            validateValueForDimensions(
                raw, var.dimensions, var.declaredType, node->line, node->col);
            if (exceedsLimit(var, raw)) {
                warnOverLimit(node->name, var.limitBits);
                return; // 原子性：保留操作前的值
            }
            var.value = deepCopy(raw);
            var.hasValue = true;
            return;
        }

        if (var.dimensions == 0) {
            throw LangError("", node->line, node->col,
                "维度冲突：0 维标量不能使用下标赋值");
        }
        if (node->indices.size() > var.dimensions) {
            throw LangError("", node->line, node->col,
                "维度冲突：下标数量超过变量的声明维度");
        }
        if (!var.hasValue) {
            throw LangError("", node->line, node->col,
                "变量尚未赋值（处于无值状态），不能使用下标赋值；请先整体 reassign");
        }

        const std::vector<std::size_t> indices = evalArrayIndices(node->indices);
        Value raw = eval(node->value.get());
        writeSubscript(
            var, node->name, indices, std::move(raw), node->line, node->col);
        return;
    }
    if (const auto* node = dynamic_cast<const FreeStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);
        if (!var.hasValue) {
            throw LangError("", node->line, node->col,
                "变量 '" + node->name + "' 当前没有值，无法 free");
        }
        // free 后不再占用内存；limit 与维度属于变量属性，继续保留。
        var.hasValue = false;
        var.value = Value{Unassigned{}};
        return;
    }
    if (const auto* node = dynamic_cast<const DeleteStmt*>(stmt)) {
        Variable* var = env_.find(node->name);
        if (var == nullptr) {
            throw LangError("", node->line, node->col,
                "变量 '" + node->name + "' 未声明，无法 delete");
        }
        env_.remove(node->name);
        return;
    }
    if (const auto* node = dynamic_cast<const AutomaticStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);
        var.declaredType = TypeKind::Automatic;
        return;
    }
    if (dynamic_cast<const AliasStmt*>(stmt)) {
        // 别名在解析期已解析为目标内建命令，运行期无需动作
        return;
    }
    if (const auto* node = dynamic_cast<const OutputStmt*>(stmt)) {
        for (const auto& value : node->values) {
            out_ << formatValue(eval(value.get()));
        }
        return;
    }
    if (const auto* node = dynamic_cast<const InputStmt*>(stmt)) {
        Variable& var = env_.require(node->name, node->line, node->col);

        std::vector<std::size_t> indices;
        if (var.dimensions == 0) {
            if (!node->indices.empty()) {
                throw LangError("", node->line, node->col,
                    "维度冲突：0 维标量不能使用下标 input");
            }
        } else {
            if (node->indices.empty()) {
                throw LangError("", node->line, node->col,
                    "类型不匹配：input 不支持整数组或数组变量");
            }
            if (node->indices.size() > var.dimensions) {
                throw LangError("", node->line, node->col,
                    "维度冲突：下标数量超过变量的声明维度");
            }
            if (node->indices.size() < var.dimensions) {
                throw LangError("", node->line, node->col,
                    "类型不匹配：input 只支持标量或完整下标的数组叶元素");
            }
            if (!var.hasValue) {
                throw LangError("", node->line, node->col,
                    "变量尚未赋值（处于无值状态），不能 input 到数组元素");
            }
            indices = evalArrayIndices(node->indices);
        }

        std::string line;
        if (!std::getline(in_, line)) {
            throw LangError("", node->line, node->col, "input 到达文件末尾（EOF）");
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        Value parsed = var.declaredType == TypeKind::Automatic
            ? parseAutomaticInput(line)
            : parseTypedInput(line, var.declaredType, node->line, node->col);

        if (var.dimensions == 0) {
            validateValueForDimensions(
                parsed, 0, var.declaredType, node->line, node->col);
            if (exceedsLimit(var, parsed)) {
                // 输入文本已被消费，但不赋值；变量保持原值。
                warnOverLimit(node->name, var.limitBits);
                return;
            }
            var.value = deepCopy(parsed);
            var.hasValue = true;
        } else {
            writeSubscript(
                var, node->name, indices, std::move(parsed), node->line, node->col);
        }
        return;
    }
    if (const auto* node = dynamic_cast<const RepeatStmt*>(stmt)) {
        executeRepeat(*node);
        return;
    }
    if (const auto* node = dynamic_cast<const IfStmt*>(stmt)) {
        Value condition = eval(node->condition.get());
        if (!isBoolean(condition)) {
            throw LangError("", node->line, node->col, "if 条件必须是布尔类型");
        }
        if (asBoolean(condition)) {
            executeBlock(node->body);
        } else {
            executeBlock(node->otherwise);
        }
        return;
    }
    if (const auto* node = dynamic_cast<const NextStmt*>(stmt)) {
        NextSignal signal;
        signal.values.reserve(node->values.size());
        for (const auto& value : node->values) {
            signal.values.push_back(eval(value.get()));
        }
        throw signal;
    }
    if (const auto* node = dynamic_cast<const ExitStmt*>(stmt)) {
        Value raw = eval(node->value.get());
        std::int64_t n = 0;
        if (isInteger(raw)) {
            n = asInteger(raw);
        } else if (isFloat(raw)) {
            double d = asFloat(raw);
            if (d < 0.0 || !std::isfinite(d) || std::trunc(d) != d) {
                throw LangError("", node->line, node->col,
                    "exit 的参数必须是非负整数或整数值浮点数");
            }
            n = static_cast<std::int64_t>(d);
        } else {
            throw LangError("", node->line, node->col,
                "exit 的参数必须是整数或整数值浮点数");
        }
        if (n < 0) {
            throw LangError("", node->line, node->col, "exit 的参数必须是非负整数");
        }
        if (n == 0) {
            return; // exit 0：继续执行下一条
        }
        int levels = 1 + blockDepth_;
        if (n > levels) {
            throw ProgramExit{};
        }
        if (n == levels) {
            throw ReturnSignal{false, {}};
        }
        throw ExitSignal{static_cast<int>(n)};
    }
    if (const auto* node = dynamic_cast<const ReturnStmt*>(stmt)) {
        if (node->value) {
            Value value = eval(node->value.get());
            throw ReturnSignal{true, value};
        }
        throw ReturnSignal{false, {}};
    }
    if (const auto* node = dynamic_cast<const InvokeStmt*>(stmt)) {
        std::vector<Value> arguments;
        arguments.reserve(node->arguments.size());
        for (const auto& argument : node->arguments) {
            arguments.push_back(eval(argument.get()));
        }
        (void)callFunction(node->name, std::move(arguments), node->line, node->col);
        return;
    }
    throw LangError("", stmt->line, stmt->col, "无法执行的语句");
}

void Interpreter::executeBlock(const Block& body) {
    BlockDepthGuard guard(&blockDepth_);
    for (const auto& statement : body) {
        try {
            execute(statement.get());
        } catch (ExitSignal& signal) {
            if (signal.levels > 1) {
                signal.levels -= 1;
                throw;
            }
            return; // 跳出本块，回到外层序列继续执行
        }
    }
}

void Interpreter::executeRepeat(const RepeatStmt& stmt) {
    // 第一轮为空；自然结束一轮后清空，next 则用其携带的值覆盖下一轮。
    std::vector<Value> values;
    RepeatValuesGuard valuesGuard(&repeatValuesStack_, &values);

    while (true) {
        bool broke = false;
        {
            BlockDepthGuard guard(&blockDepth_);
            try {
                for (const auto& statement : stmt.body) {
                    execute(statement.get());
                }
                values.clear();
            } catch (NextSignal& signal) {
                values = std::move(signal.values);
            } catch (ExitSignal& signal) {
                if (signal.levels > 1) {
                    signal.levels -= 1;
                    throw;
                }
                broke = true;
                values.clear();
            }
        }
        if (broke) break;
    }
}

ReturnSignal Interpreter::callFunction(
    const std::string& name,
    std::vector<Value> arguments,
    int line,
    int col) {
    const FunctionDef* function = env_.findFunction(name);
    if (function == nullptr) {
        throw LangError("", line, col, "调用了未定义函数: " + name);
    }

    env_.pushFrame();
    FrameGuard guard(&env_);
    for (std::size_t i = 0; i < function->parameters.size(); ++i) {
        env_.declare(
            function->parameters[i],
            TypeKind::Automatic,
            true,
            arguments[i],
            valueDimensions(arguments[i]));
    }

    try {
        for (const auto& statement : function->body) {
            execute(statement.get());
        }
    } catch (const ReturnSignal& returned) {
        return returned;
    }
    return ReturnSignal{false, {}};
}

Value Interpreter::parseTypedInput(const std::string& line, TypeKind type, int lline, int col) {
    switch (type) {
    case TypeKind::Integer: {
        if (line.empty()) {
            throw LangError("", lline, col, "空行无法解析为 integer");
        }
        try {
            std::size_t processed = 0;
            std::int64_t value = std::stoll(line, &processed);
            if (processed != line.size()) {
                throw LangError("", lline, col, "无法将输入解析为 integer: " + line);
            }
            return value;
        } catch (const std::exception&) {
            throw LangError("", lline, col, "无法将输入解析为 integer: " + line);
        }
    }
    case TypeKind::Float: {
        if (line.empty()) {
            throw LangError("", lline, col, "空行无法解析为 float");
        }
        try {
            std::size_t processed = 0;
            double value = std::stod(line, &processed);
            if (processed != line.size()) {
                throw LangError("", lline, col, "无法将输入解析为 float: " + line);
            }
            return value;
        } catch (const std::exception&) {
            throw LangError("", lline, col, "无法将输入解析为 float: " + line);
        }
    }
    case TypeKind::Boolean: {
        if (line == "true") return true;
        if (line == "false") return false;
        if (line.empty()) {
            throw LangError("", lline, col, "空行无法解析为 boolean");
        }
        throw LangError("", lline, col, "boolean 只接受 true 或 false，得到: " + line);
    }
    case TypeKind::String:
        return line;
    case TypeKind::Automatic:
        return line;
    }
    return line;
}

Value Interpreter::parseAutomaticInput(const std::string& line) {
    if (line.empty()) {
        return std::string(""); // 空行得到空字符串
    }
    std::string trimmed = trim(line);
    if (trimmed.empty()) {
        return std::string("");
    }
    try {
        std::size_t processed = 0;
        std::int64_t integer = std::stoll(trimmed, &processed);
        if (processed == trimmed.size()) {
            return integer;
        }
    } catch (const std::exception&) {
        // 继续尝试 float
    }
    try {
        std::size_t processed = 0;
        double floating = std::stod(trimmed, &processed);
        if (processed == trimmed.size()) {
            return floating;
        }
    } catch (const std::exception&) {
        // 回退到字符串
    }
    return line; // 保留原始行内容
}
