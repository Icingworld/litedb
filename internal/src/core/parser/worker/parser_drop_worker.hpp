#pragma once

#include <expected>
#include <memory>

#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

namespace ast
{

class StatementNode;

} // namespace ast

// DROP 语句解析工作器
class ParserDropWorker
{
public:
    explicit ParserDropWorker(ParserContext & context) noexcept;

public:
    // 解析 DROP 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_statement();

private:
    // 解析 DROP DATABASE 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_database_statement(TokenLocation location);

    // 解析 DROP COLLECTION 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_collection_statement(TokenLocation location);

    // 解析 DROP INDEX 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_index_statement(TokenLocation location);

    // 解析 DROP VECTOR INDEX 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_vector_index_statement(TokenLocation location);

private:
    ParserContext & context_;
    ParserSchemaHelper schema_helper_;
};

} // namespace litedb::core::parser
