#include "../src/ast.h"
#include "../src/error.h"
#include "../src/lexer.h"
#include "../src/parser.h"

#include <cassert>
#include <string>

namespace {

Program parseText(const std::string& text) {
    Source source{"parser_smoke.pace", text};
    Lexer lexer(source);
    Parser parser(source.file, lexer.tokenize());
    return parser.parse();
}

void expectError(const std::string& text, const std::string& fragment) {
    try {
        (void)parseText(text);
    } catch (const LangError& error) {
        assert(std::string(error.what()).find(fragment) != std::string::npos);
        return;
    }
    assert(false && "expected LangError");
}

} // namespace

int main() {
    Program program = parseText(R"LANG(function add a, b
  return a
function main
  initialize integer c 3
  output invoke add 1, 2, c
  if not 1 = 2
    output "yes"
)LANG");

    assert(program.functions.size() == 2);
    const FunctionDef& mainFunction = *program.functions[1];
    assert(mainFunction.body.size() == 3);

    const auto& output = dynamic_cast<const OutputStmt&>(*mainFunction.body[1]);
    assert(output.values.size() == 2);
    const auto& invocation = dynamic_cast<const InvokeExpr&>(*output.values[0]);
    assert(invocation.name == "add");
    assert(invocation.arguments.size() == 2);
    const auto& thirdOutput = dynamic_cast<const VarRef&>(*output.values[1]);
    assert(thirdOutput.name == "c");

    const auto& ifStatement = dynamic_cast<const IfStmt&>(*mainFunction.body[2]);
    const auto& notExpression = dynamic_cast<const Unary&>(*ifStatement.condition);
    assert(notExpression.op == TokenKind::Not);
    const auto& comparison = dynamic_cast<const Binary&>(*notExpression.operand);
    assert(comparison.op == TokenKind::Equal);

    expectError(R"LANG(function main
  output 1 < 2 < 3
)LANG", "链式比较");

    expectError(R"LANG(function main
  alias output value
  initialize integer value 1
)LANG", "冲突");

    expectError(R"LANG(function main
  initialize integer value 1
  alias output value
)LANG", "冲突");

    expectError(R"LANG(function main
  initialize integer a limit 1024, b limit 32
)LANG", "缺少类型声明");

    expectError(R"LANG(function main
  output invoke add 1
function add a, b
  return a
)LANG", "参数个数不匹配");

    expectError(R"LANG(function main
  invoke add 1, 2, 3
function add a, b
  return a
)LANG", "参数个数不匹配");

    Program statements = parseText(R"LANG(function helper p
  alias output show
  show p
  return
function main
  initialize automatic a 1, string s "x"
  reassign a 2
  free a
  automatic a
  input s
  repeat
    exit 1
  if true
    return 1
  otherwise
    invoke helper "x"
  exit 0
)LANG");
    assert(statements.functions.size() == 2);
    assert(statements.functions[1]->body.size() == 8);

    Program emptyBlocks = parseText(R"LANG(function empty
function main
  if true
  otherwise
  repeat
  exit 2
)LANG");
    assert(emptyBlocks.functions.size() == 2);
    assert(emptyBlocks.functions[0]->body.empty());

    expectError(R"LANG(function main
  if true
    alias output show
)LANG", "函数体顶层");

    Program attributes = parseText(R"LANG(function main
  initialize automatic a [1, 2]
  initialize integer i 1
  output a_i, a_(i), a_size, a_1_quantity
)LANG");
    const auto& attributeBody = attributes.functions[0]->body;
    const auto& attributeOutput =
        dynamic_cast<const OutputStmt&>(*attributeBody[2]);
    assert(attributeOutput.values.size() == 4);
    const auto& bareIndex = dynamic_cast<const VarRef&>(*attributeOutput.values[0]);
    assert(bareIndex.name == "a" && bareIndex.indices.size() == 1);
    const auto& sizeAttribute =
        dynamic_cast<const AttributeExpr&>(*attributeOutput.values[2]);
    assert(sizeAttribute.name == "a" && sizeAttribute.indices.empty());
    assert(sizeAttribute.kind == AttributeKind::Size);
    const auto& quantityAttribute =
        dynamic_cast<const AttributeExpr&>(*attributeOutput.values[3]);
    assert(quantityAttribute.name == "a" && quantityAttribute.indices.size() == 1);
    assert(quantityAttribute.kind == AttributeKind::Quantity);

    expectError(R"LANG(function main
  initialize integer size 1
)LANG", "关键字不能作为标识符");

    expectError(R"LANG(function main
  initialize integer a 1
  reassign a_size 1
)LANG", "只读");

    expectError(R"LANG(function main
  initialize automatic a [1]
  output a_size_1
)LANG", "下标链末尾");

    expectError(R"LANG(function main
  initialize integer a 1
  automatic a_quantity
)LANG", "只读");

    return 0;
}
