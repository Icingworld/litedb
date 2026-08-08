#pragma once

#include <expected>
#include <memory>

#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/token.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

namespace ast
{

class StatementNode;

} // namespace ast

// CREATE 语句解析工作器
class ParserCreateWorker
{
public:
    explicit ParserCreateWorker(ParserContext & context) noexcept;

public:
    // 解析 CREATE 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_create_statement();

private:
    // 解析 CREATE DATABASE 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_create_database_statement(TokenLocation location);

    // 解析 CREATE COLLECTION 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_create_collection_statement(TokenLocation location);

    // 解析 CREATE INDEX 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_create_index_statement(TokenLocation location, bool unique);

    // 解析 CREATE VINDEX 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_create_vector_index_statement(TokenLocation location);

private:
    ParserContext & context_;                   // 解析上下文
    ParserSchemaHelper schema_helper_;          // schema 和通用语法解析辅助
};

} // namespace litedb::core::parser
