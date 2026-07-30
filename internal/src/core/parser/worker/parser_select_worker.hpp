#pragma once

#include <expected>
#include <memory>

#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"

namespace litedb::core::parser
{

namespace ast
{

class ExpressionNode;
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
    /**
     * @brief 解析 SELECT 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_select_statement();

private:
    /**
     * @brief 解析 SELECT 列表项
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
    parse_select_item();

private:
    ParserContext & context_;                   ///< 解析上下文
};

} // namespace litedb::core::parser
