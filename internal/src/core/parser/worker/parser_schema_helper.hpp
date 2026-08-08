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

// schema 和通用语法解析辅助器
class ParserSchemaHelper
{
public:
    explicit ParserSchemaHelper(ParserContext & context) noexcept;

public:
    // 解析标识符字符串
    [[nodiscard]]
    std::expected<std::string, ParserError> parse_identifier_string(std::string_view message);

    // 解析整数值
    [[nodiscard]]
    std::expected<std::size_t, ParserError> parse_integer_value(std::string_view message);

    // 解析数据类型
    [[nodiscard]]
    std::expected<common::LogicalType, ParserError> parse_data_type();

    // 解析列定义
    [[nodiscard]]
    std::expected<ast::ColumnDefinitionSyntax, ParserError> parse_column_definition();

    // 解析 IF NOT EXISTS
    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_not_exists();

    // 解析 IF EXISTS
    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_exists();

private:
    ParserContext & context_;
    ParserExpressionWorker expression_worker_;
};

} // namespace litedb::core::parser
