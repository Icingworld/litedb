#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/identifier_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/statement/select_statement.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{

using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_expression_nodes()
{
    auto left = std::make_unique<IdentifierExpression>("age", AstNodeLocation {1, 7});
    auto right = std::make_unique<LiteralExpression>(TokenType::IntegerLiteral, "18", AstNodeLocation {1, 14});
    BinaryExpression expression(std::move(left), TokenType::GreaterEqual, std::move(right), AstNodeLocation {1, 7});

    require(expression.kind() == AstNodeKind::Binary, "binary expression kind mismatch");
    require(expression.location().line == 1, "binary expression line mismatch");
    require(expression.location().column == 7, "binary expression column mismatch");
    require(expression.left().kind() == AstNodeKind::Identifier, "left expression kind mismatch");
    require(expression.right().kind() == AstNodeKind::Literal, "right expression kind mismatch");
}

void test_statement_nodes()
{
    SelectStatement::SelectList select_list;
    select_list.push_back(std::make_unique<IdentifierExpression>("name", AstNodeLocation {1, 8}));

    SelectStatement statement(
        std::move(select_list),
        "users",
        nullptr,
        {},
        10,
        {},
        AstNodeLocation {1, 1}
    );

    require(statement.kind() == AstNodeKind::Select, "select statement kind mismatch");
    require(statement.location().line == 1, "select statement line mismatch");
    require(statement.collection() == "users", "select collection mismatch");
    require(statement.select_list().size() == 1, "select list size mismatch");
    require(statement.where() == nullptr, "select where should be empty");
    require(statement.limit().has_value() && statement.limit().value() == 10, "select limit mismatch");
}

} // namespace

int main()
{
    try {
        test_expression_nodes();
        test_statement_nodes();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
