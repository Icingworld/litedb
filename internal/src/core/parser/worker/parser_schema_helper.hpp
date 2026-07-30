#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include "core/parser/ast/column_definition.hpp"
#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/worker/parser_expression_worker.hpp"

namespace litedb::core::parser
{

/**
 * @brief schema 和通用语法解析辅助器
 */
class ParserSchemaHelper
{
public:
    explicit ParserSchemaHelper(ParserContext & context);

public:
    /**
     * @brief 解析标识符字符串
     * @param message 错误消息
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::string, ParserError>
    parse_identifier_string(std::string_view message);

    /**
     * @brief 解析整数值
     * @param message 错误消息
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::size_t, ParserError>
    parse_integer_value(std::string_view message);

    /**
     * @brief 解析数据类型
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<common::LogicalType, ParserError>
    parse_data_type();

    /**
     * @brief 解析列定义
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<ast::ColumnDefinitionSyntax, ParserError>
    parse_column_definition();

    /**
     * @brief 解析 IF NOT EXISTS
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<bool, ParserError>
    parse_if_not_exists();

    /**
     * @brief 解析 IF EXISTS
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<bool, ParserError>
    parse_if_exists();

private:
    ParserContext & context_;                   ///< 解析上下文
    ParserExpressionWorker expression_worker_;  ///< 表达式解析工作器
};

} // namespace litedb::core::parser
