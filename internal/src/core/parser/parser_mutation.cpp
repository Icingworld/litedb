#include "core/parser/parser_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/update_statement.hpp"

namespace litedb::core::parser
{

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_insert_statement()
{
    // 保存并消耗 INSERT 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 期望 INTO 关键字
    auto into = consume(TokenType::Into, "Expected INTO after INSERT");
    if (!into.has_value()) [[unlikely]] {
        return std::unexpected(into.error());
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 解析列列表
    ast::InsertStatement::ColumnList columns;
    // 尝试匹配 (
    if (match(TokenType::LeftParen)) {
        // 检查是否为空列表
        if (check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one column name"));
        }

        // 解析列名称列表
        while (true) {
            // 解析列名称
            auto column = parse_identifier_string("Expected column name");
            if (!column.has_value()) [[unlikely]] {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after column list");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
    }

    // 期望 VALUES 关键字
    auto values = consume(TokenType::Values, "Expected VALUES after INSERT target");
    if (!values.has_value()) [[unlikely]] {
        return std::unexpected(values.error());
    }
    // 期望 (
    auto left_paren = consume(TokenType::LeftParen, "Expected '(' before values");
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(left_paren.error());
    }
    // 检查是否为空列表
    if (check(TokenType::RightParen)) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one value"));
    }

    // 解析值列表
    ast::InsertStatement::ValueList value_list;
    while (true) {
        // 解析表达式
        auto value = parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        value_list.push_back(std::move(value.value()));

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 期望 )
    auto right_paren = consume(TokenType::RightParen, "Expected ')' after values");
    if (!right_paren.has_value()) [[unlikely]] {
        return std::unexpected(right_paren.error());
    }

    return std::make_unique<ast::InsertStatement>(
        std::move(collection.value()),
        std::move(columns),
        std::move(value_list),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_update_statement()
{
    // 保存并消耗 UPDATE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 期望 SET 关键字
    auto set = consume(TokenType::Set, "Expected SET after collection name");
    if (!set.has_value()) [[unlikely]] {
        return std::unexpected(set.error());
    }

    // 解析赋值列表
    ast::UpdateStatement::AssignmentList assignments;
    while (true) {
        // 解析列名称
        auto column = parse_identifier_string("Expected column name");
        if (!column.has_value()) [[unlikely]] {
            return std::unexpected(column.error());
        }

        // 期望 = 关键字
        auto equal = consume(TokenType::Equal, "Expected '=' after column name");
        if (!equal.has_value()) {
            return std::unexpected(equal.error());
        }

        // 解析表达式
        auto value = parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }

        assignments.push_back(ast::Assignment {
            std::move(column.value()),
            std::move(value.value()),
        });

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 尝试匹配 WHERE 关键字
    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        // 解析表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::UpdateStatement>(
        std::move(collection.value()),
        std::move(assignments),
        std::move(where),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_delete_statement()
{
    // 保存并消耗 DELETE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 期望 FROM 关键字
    auto from = consume(TokenType::From, "Expected FROM after DELETE");
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(from.error());
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 尝试匹配 WHERE 关键字
    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        // 解析表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::DeleteStatement>(
        std::move(collection.value()),
        std::move(where),
        ast_location(location)
    );
}

} // namespace litedb::core::parser
