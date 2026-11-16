#include "value.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {

bool isIntegerValued(double value) {
    return std::isfinite(value) && std::trunc(value) == value;
}

std::string typeName(const Value& value) {
    if (isUnassigned(value)) return "unassigned";
    if (std::holds_alternative<std::int64_t>(value)) return "integer";
    if (std::holds_alternative<double>(value)) return "float";
    if (std::holds_alternative<std::string>(value)) return "string";
    if (std::holds_alternative<bool>(value)) return "boolean";
    return "array";
}

} // namespace

bool isUnassigned(const Value& value) { return std::holds_alternative<Unassigned>(value); }
bool isInteger(const Value& value) { return std::holds_alternative<std::int64_t>(value); }
bool isFloat(const Value& value) { return std::holds_alternative<double>(value); }
bool isNumeric(const Value& value) {
    return std::holds_alternative<std::int64_t>(value) ||
           std::holds_alternative<double>(value);
}
bool isString(const Value& value) { return std::holds_alternative<std::string>(value); }
bool isBoolean(const Value& value) { return std::holds_alternative<bool>(value); }
bool isArray(const Value& value) { return std::holds_alternative<ArrayPtr>(value); }
bool isScalar(const Value& value) { return !isArray(value) && !isUnassigned(value); }

std::int64_t asInteger(const Value& value) { return std::get<std::int64_t>(value); }
double asFloat(const Value& value) { return std::get<double>(value); }
bool asBoolean(const Value& value) { return std::get<bool>(value); }
const std::string& asString(const Value& value) { return std::get<std::string>(value); }
const Array& asArray(const Value& value) { return *std::get<ArrayPtr>(value); }
Array& asArray(Value& value) { return *std::get<ArrayPtr>(value); }

std::size_t valueDimensions(const Value& value) {
    return isArray(value) ? asArray(value).dimensions : 0;
}

Value makeArray(std::size_t dimensions) {
    auto array = std::make_shared<Array>();
    array->dimensions = dimensions;
    return Value(std::move(array));
}

Value deepCopy(const Value& value) {
    if (!isArray(value)) {
        return value;
    }
    auto copy = std::make_shared<Array>();
    copy->dimensions = asArray(value).dimensions;
    copy->elements.reserve(asArray(value).elements.size());
    for (const Value& element : asArray(value).elements) {
        copy->elements.push_back(deepCopy(element));
    }
    return Value(std::move(copy));
}

Value convertTo(const Value& value, TypeKind target, int line, int col) {
    switch (target) {
    case TypeKind::Integer:
        if (isInteger(value)) return value;
        if (isFloat(value)) {
            double d = asFloat(value);
            if (!isIntegerValued(d) || d < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
                d > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                throw LangError("", line, col,
                    "浮点数 " + formatValue(value) +
                    " 转换为 integer 时小数部分须为 0 且落在 int64 范围内");
            }
            return static_cast<std::int64_t>(d);
        }
        if (isBoolean(value)) return asBoolean(value) ? std::int64_t(1) : std::int64_t(0);
        throw LangError("", line, col,
            "无法将 " + typeName(value) + " 转换为 integer");

    case TypeKind::Float:
        if (isInteger(value)) return static_cast<double>(asInteger(value));
        if (isFloat(value)) return value;
        if (isBoolean(value)) return asBoolean(value) ? 1.0 : 0.0;
        throw LangError("", line, col,
            "无法将 " + typeName(value) + " 转换为 float");

    case TypeKind::Boolean:
        if (isInteger(value)) return asInteger(value) != 0;
        if (isFloat(value)) return asFloat(value) != 0.0;
        if (isBoolean(value)) return value;
        throw LangError("", line, col,
            "无法将 " + typeName(value) + " 转换为 boolean");

    case TypeKind::String:
        if (isInteger(value)) return std::to_string(asInteger(value));
        if (isFloat(value)) return formatValue(value);
        if (isBoolean(value)) return asBoolean(value) ? "true" : "false";
        if (isString(value)) return value;
        throw LangError("", line, col,
            "无法将 " + typeName(value) + " 转换为 string");

    case TypeKind::Automatic:
        return deepCopy(value);
    }
    throw LangError("", line, col, "未知的类型转换目标");
}

namespace {

double toDouble(const Value& value) {
    if (isInteger(value)) return static_cast<double>(asInteger(value));
    return asFloat(value);
}

} // namespace

Value applyBinary(TokenKind op, const Value& left, const Value& right, int line, int col) {
    switch (op) {
    case TokenKind::Plus:
    case TokenKind::Minus:
    case TokenKind::Star:
    case TokenKind::Slash: {
        // 字符串拼接：仅 '+' 且两侧均为字符串时允许
        if (op == TokenKind::Plus && isString(left) && isString(right)) {
            return asString(left) + asString(right);
        }
        // 字符串与数值混合（或 '-'/'*'/'/' 涉及字符串）一律类型不匹配
        if (isString(left) || isString(right)) {
            if (op == TokenKind::Plus) {
                throw LangError("", line, col, "字符串只能与字符串拼接（'+'）");
            }
            throw LangError("", line, col, "字符串不能参与 '-'/'*'/'/' 运算");
        }
        if (!isNumeric(left) || !isNumeric(right)) {
            throw LangError("", line, col, "算术运算 '+'/'-'/'*'/'/' 要求两侧均为数值");
        }

        // 除法恒返回 double
        if (op == TokenKind::Slash) {
            double a = toDouble(left);
            double b = toDouble(right);
            if (b == 0.0) {
                throw LangError("", line, col, "除数不能为 0（除零错误）");
            }
            double result = a / b;
            if (!std::isfinite(result)) {
                throw LangError("", line, col, "浮点除法结果溢出");
            }
            return result;
        }

        if (isInteger(left) && isInteger(right)) {
            std::int64_t a = asInteger(left);
            std::int64_t b = asInteger(right);
            std::int64_t result = 0;
#if defined(__GNUC__) || defined(__clang__)
            bool overflow = false;
            if (op == TokenKind::Plus) overflow = __builtin_add_overflow(a, b, &result);
            else if (op == TokenKind::Minus) overflow = __builtin_sub_overflow(a, b, &result);
            else overflow = __builtin_mul_overflow(a, b, &result);
#else
            bool overflow = false;
#if defined(__SIZEOF_INT128__)
            __int128 tmp = static_cast<__int128>(a) * static_cast<__int128>(b);
            if (op == TokenKind::Mul && (tmp > std::numeric_limits<std::int64_t>::max() ||
                                         tmp < std::numeric_limits<std::int64_t>::min())) {
                overflow = true;
            } else if (op != TokenKind::Mul) {
                if (op == TokenKind::Plus) {
                    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
                        (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b)) overflow = true;
                } else {
                    if ((b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) ||
                        (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b)) overflow = true;
                }
                if (!overflow) result = (op == TokenKind::Plus) ? a + b : a - b;
            }
#else
            result = (op == TokenKind::Plus) ? a + b : (op == TokenKind::Minus) ? a - b : a * b;
            if (op != TokenKind::Mul && b != 0 && result / b != a) overflow = true;
            if (op == TokenKind::Mul && a != 0 && result / a != b) overflow = true;
#endif
#endif
            if (overflow) {
                throw LangError("", line, col, "整数运算溢出（超出 int64 范围）");
            }
            return result;
        }

        // 至少一侧为 double
        double a = toDouble(left);
        double b = toDouble(right);
        double result = 0.0;
        if (op == TokenKind::Plus) result = a + b;
        else if (op == TokenKind::Minus) result = a - b;
        else result = a * b;
        if (!std::isfinite(result)) {
            throw LangError("", line, col, "浮点运算结果溢出（inf）");
        }
        return result;
    }

    case TokenKind::Equal:
    case TokenKind::NotEqual:
    case TokenKind::Less:
    case TokenKind::LessEqual:
    case TokenKind::Greater:
    case TokenKind::GreaterEqual: {
        bool result = false;
        if (isNumeric(left)) {
            if (!isNumeric(right)) {
                throw LangError("", line, col, "数值只能与数值比较");
            }
            double a = toDouble(left);
            double b = toDouble(right);
            switch (op) {
            case TokenKind::Equal: result = (a == b); break;
            case TokenKind::NotEqual: result = (a != b); break;
            case TokenKind::Less: result = (a < b); break;
            case TokenKind::LessEqual: result = (a <= b); break;
            case TokenKind::Greater: result = (a > b); break;
            case TokenKind::GreaterEqual: result = (a >= b); break;
            default: break;
            }
        } else if (isString(left)) {
            if (!isString(right)) {
                throw LangError("", line, col, "字符串只能与字符串比较");
            }
            const std::string& a = asString(left);
            const std::string& b = asString(right);
            int cmp = a.compare(b);
            switch (op) {
            case TokenKind::Equal: result = (cmp == 0); break;
            case TokenKind::NotEqual: result = (cmp != 0); break;
            case TokenKind::Less: result = (cmp < 0); break;
            case TokenKind::LessEqual: result = (cmp <= 0); break;
            case TokenKind::Greater: result = (cmp > 0); break;
            case TokenKind::GreaterEqual: result = (cmp >= 0); break;
            default: break;
            }
        } else if (isBoolean(left)) {
            if (!isBoolean(right)) {
                throw LangError("", line, col, "布尔只能与布尔比较");
            }
            bool a = asBoolean(left);
            bool b = asBoolean(right);
            switch (op) {
            case TokenKind::Equal: result = (a == b); break;
            case TokenKind::NotEqual: result = (a != b); break;
            case TokenKind::Less: result = (a < b); break;
            case TokenKind::LessEqual: result = (a <= b); break;
            case TokenKind::Greater: result = (a > b); break;
            case TokenKind::GreaterEqual: result = (a >= b); break;
            default: break;
            }
        } else {
            throw LangError("", line, col, "不支持的比较操作数类型");
        }
        return result;
    }

    default:
        throw LangError("", line, col, "不支持的二元运算符");
    }
}

Value applyUnary(TokenKind op, const Value& operand, int line, int col) {
    if (op == TokenKind::Minus) {
        if (isInteger(operand)) {
            std::int64_t v = asInteger(operand);
            if (v == std::numeric_limits<std::int64_t>::min()) {
                throw LangError("", line, col, "整数取负溢出");
            }
            return static_cast<std::int64_t>(-v);
        }
        if (isFloat(operand)) return -asFloat(operand);
        throw LangError("", line, col, "一元负号 '-' 只接受数值");
    }
    if (op == TokenKind::Not) {
        if (!isBoolean(operand)) {
            throw LangError("", line, col, "not 只接受布尔类型");
        }
        return !asBoolean(operand);
    }
    throw LangError("", line, col, "不支持的一元运算符");
}

namespace {

std::string quoteArrayString(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        switch (c) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        default: result.push_back(c); break;
        }
    }
    result.push_back('"');
    return result;
}

std::string formatArray(const Array& array) {
    std::string result = "[";
    for (std::size_t i = 0; i < array.elements.size(); ++i) {
        if (i > 0) result += ", ";
        const Value& element = array.elements[i];
        if (isString(element)) {
            result += quoteArrayString(asString(element));
        } else {
            result += formatValue(element);
        }
    }
    result += "]";
    return result;
}

} // namespace

std::string formatValue(const Value& value) {
    if (isUnassigned(value)) return "unassigned";
    if (isInteger(value)) return std::to_string(asInteger(value));
    if (isFloat(value)) {
        double d = asFloat(value);
        char buffer[64];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), d);
        std::string text(buffer, result.ptr);
        if (text.find('.') == std::string::npos &&
            text.find('e') == std::string::npos &&
            text.find('E') == std::string::npos) {
            text += ".0";
        }
        return text;
    }
    if (isBoolean(value)) return asBoolean(value) ? "true" : "false";
    if (isArray(value)) return formatArray(asArray(value));
    return asString(value);
}
