#pragma once

#include <expected>
#include <memory>

#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"

namespace litedb::core::parser
{

namespace ast
{

class StatementNode;

} // namespace ast

// SHOW 语句解析工作器
class ParserShowWorker
{
public:
    explicit ParserShowWorker(ParserContext & context) noexcept;

public:
    // 解析 SHOW 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_show_statement();

private:
    // 解析 SHOW DATABASES 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_show_databases_statement(TokenLocation location);

    // 解析 SHOW COLLECTIONS 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_show_collections_statement(TokenLocation location);

    // 解析 SHOW INDEXES 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_show_indexes_statement(TokenLocation location);

    // 解析 SHOW VECTOR INDEXES 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_show_vector_indexes_statement(TokenLocation location);

private:
    ParserContext & context_;   // 解析上下文
};

} // namespace litedb::core::parser
