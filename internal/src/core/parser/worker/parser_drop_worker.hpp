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

/**
 * @brief DROP 语句解析工作器
 */
class ParserDropWorker
{
public:
    explicit ParserDropWorker(ParserContext & context);

public:
    /**
     * @brief 解析 DROP 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_statement();

private:
    /**
     * @brief 解析 DROP DATABASE 语句
     * @param location 语句位置
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_database_statement(TokenLocation location);

    /**
     * @brief 解析 DROP COLLECTION 语句
     * @param location 语句位置
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_collection_statement(TokenLocation location);

    /**
     * @brief 解析 DROP INDEX 语句
     * @param location 语句位置
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_index_statement(TokenLocation location);

    /**
     * @brief 解析 DROP VECTOR INDEX 语句
     * @param location 语句位置
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_drop_vector_index_statement(TokenLocation location);

private:
    ParserContext & context_;                   ///< 解析上下文
    ParserSchemaHelper schema_helper_;          ///< schema 和通用语法解析辅助
};

} // namespace litedb::core::parser
