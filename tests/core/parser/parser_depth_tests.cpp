#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "core/parser/parser.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace
{

using namespace litedb::core::parser;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::unique_ptr<ast::StatementNode> parse_ok(std::string sql)
{
    Parser parser {std::move(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

ParserError parse_error(std::string sql)
{
    Parser parser {std::move(sql)};
    auto result = parser.parse();
    require(!result.has_value(), "statement should fail to parse");
    return std::move(result.error());
}

TokenLocation error_location(const ParserError & error)
{
    const auto * context = error.context<ParserErrorContext>();
    require(context != nullptr, "parser error should retain token location");
    return context->location;
}

std::string select_sql(std::string expression)
{
    return "SELECT " + std::move(expression) + " FROM users;";
}

std::string unary_expression(std::string_view op, std::size_t count)
{
    std::string expression;
    expression.reserve(count * (op.size() + 1) + 3);
    for (std::size_t index = 0; index < count; ++index) {
        expression.append(op);
        expression.push_back(' ');
    }
    expression += "age";
    return expression;
}

std::string parenthesized_expression(std::size_t count)
{
    return std::string(count, '(') + "age" + std::string(count, ')');
}

std::string binary_expression(std::size_t operand_count)
{
    std::string expression {"age"};
    expression.reserve(operand_count * 6);
    for (std::size_t index = 1; index < operand_count; ++index) {
        expression += " + age";
    }
    return expression;
}

std::string nested_function_expression(std::size_t count)
{
    std::string expression {"age"};
    for (std::size_t index = 0; index < count; ++index) {
        expression = "f(" + std::move(expression) + ")";
    }
    return expression;
}

std::string nested_vector_expression(std::size_t count)
{
    std::string expression {"age"};
    for (std::size_t index = 0; index < count; ++index) {
        expression = "[" + std::move(expression) + "]";
    }
    return expression;
}

std::string nested_in_expression(std::size_t count)
{
    std::string expression {"age"};
    for (std::size_t index = 0; index < count; ++index) {
        expression = "age IN (" + std::move(expression) + ")";
    }
    return expression;
}

std::string repeated_list(std::size_t count)
{
    std::string values;
    values.reserve(count * 4);
    for (std::size_t index = 0; index < count; ++index) {
        if (index != 0) {
            values += ", ";
        }
        values += "age";
    }
    return values;
}

void require_depth_error(
    const ParserError & error,
    std::string_view message
)
{
    require(
        error.is(ParserErrorCode::ExpressionDepthLimitExceeded),
        "expression depth error code mismatch"
    );
    require(error.message() == message, "expression depth error message mismatch");
    require(error_location(error).line == 1, "expression depth error line mismatch");
}

void test_syntax_nesting_depth()
{
    parse_ok(select_sql(parenthesized_expression(256)));

    auto error = parse_error(select_sql(parenthesized_expression(257)));
    require_depth_error(
        error,
        "Maximum expression nesting depth exceeded (limit: 256)"
    );
    require(error_location(error).column == 264, "parenthesis error column mismatch");

    auto functions = parse_error(select_sql(nested_function_expression(257)));
    require_depth_error(
        functions,
        "Maximum expression nesting depth exceeded (limit: 256)"
    );
}

void test_unary_and_binary_ast_depth()
{
    parse_ok(select_sql(unary_expression("-", 255)));
    auto unary_error = parse_error(select_sql(unary_expression("-", 256)));
    require_depth_error(
        unary_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );
    require(error_location(unary_error).column == 8, "unary error column mismatch");

    parse_ok(select_sql(unary_expression("NOT", 255)));
    auto not_error = parse_error(select_sql(unary_expression("NOT", 256)));
    require_depth_error(
        not_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    parse_ok(select_sql(binary_expression(256)));
    auto binary_error = parse_error(select_sql(binary_expression(257)));
    require_depth_error(
        binary_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );
}

void test_composite_ast_depth()
{
    parse_ok(select_sql(nested_function_expression(255)));
    auto function_error = parse_error(select_sql(nested_function_expression(256)));
    require_depth_error(
        function_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    parse_ok(select_sql(nested_vector_expression(255)));
    auto vector_error = parse_error(select_sql(nested_vector_expression(256)));
    require_depth_error(
        vector_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    parse_ok(select_sql(nested_in_expression(255)));
    auto in_error = parse_error(select_sql(nested_in_expression(256)));
    require_depth_error(
        in_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    auto like_error = parse_error(select_sql(
        unary_expression("-", 255) + " LIKE 'x'"
    ));
    require_depth_error(
        like_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    auto between_error = parse_error(select_sql(
        unary_expression("-", 255) + " BETWEEN 1 AND 2"
    ));
    require_depth_error(
        between_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    auto not_in_error = parse_error(select_sql(
        "NOT " + nested_in_expression(255)
    ));
    require_depth_error(
        not_in_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    parse_ok(select_sql(unary_expression("-", 254) + " AS value"));
    auto alias_error = parse_error(select_sql(
        unary_expression("-", 255) + " AS value"
    ));
    require_depth_error(
        alias_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );
}

void test_shallow_width_and_stress()
{
    const auto values = repeated_list(300);
    parse_ok(select_sql("f(" + values + ")"));
    parse_ok(select_sql("[" + values + "]"));
    parse_ok(select_sql("age IN (" + values + ")"));

    auto unary_error = parse_error(select_sql(unary_expression("-", 4096)));
    require_depth_error(
        unary_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );

    auto binary_error = parse_error(select_sql(binary_expression(4096)));
    require_depth_error(
        binary_error,
        "Maximum AST expression depth exceeded (limit: 256)"
    );
}

} // namespace

int main()
{
    try {
        test_syntax_nesting_depth();
        test_unary_and_binary_ast_depth();
        test_composite_ast_depth();
        test_shallow_width_and_stress();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
