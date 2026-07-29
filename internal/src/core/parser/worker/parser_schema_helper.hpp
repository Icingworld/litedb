#pragma once

#include <cstddef>
#include <expected>
#include <memory>
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
    [[nodiscard]]
    std::expected<std::string, ParserError> parse_identifier_string(std::string_view message);

    [[nodiscard]]
    std::expected<std::size_t, ParserError> parse_integer_value(std::string_view message);

    [[nodiscard]]
    std::expected<common::LogicalType, ParserError> parse_data_type();

    [[nodiscard]]
    std::expected<ast::ColumnDefinitionSyntax, ParserError> parse_column_definition();

    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_not_exists();

    [[nodiscard]]
    std::expected<bool, ParserError> parse_if_exists();

private:
    ParserContext & context_;
    ParserExpressionWorker expression_worker_;
};

} // namespace litedb::core::parser
