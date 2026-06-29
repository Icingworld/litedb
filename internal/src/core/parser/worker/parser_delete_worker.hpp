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
 * @brief DELETE 语句解析工作器
 */
class ParserDeleteWorker
{
public:
    explicit ParserDeleteWorker(ParserContext & context);

public:
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_delete_statement();

private:
    ParserContext & context_;   ///< 解析上下文
};

} // namespace litedb::core::parser
