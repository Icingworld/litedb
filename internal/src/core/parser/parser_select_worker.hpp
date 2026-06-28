#pragma once

#include <expected>
#include <memory>

#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/parser_expression_worker.hpp"
#include "core/parser/parser_schema_worker.hpp"

namespace litedb::core::parser
{

namespace ast
{

class StatementNode;

} // namespace ast

/**
 * @brief SELECT 语句解析工作器
 */
class ParserSelectWorker
{
public:
    explicit ParserSelectWorker(ParserContext & context);

public:
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_select_statement();

private:
    ParserContext & context_;                   ///< 解析上下文
    ParserExpressionWorker expression_worker_;  ///< 表达式解析工作器
    ParserSchemaWorker schema_worker_;          ///< schema 解析工作器
};

} // namespace litedb::core::parser
