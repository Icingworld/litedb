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
    ParserContext & context_;
};

} // namespace litedb::core::parser
