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

/**
 * @brief DROP 语句解析工作器
 */
class ParserDropWorker
{
public:
    explicit ParserDropWorker(ParserContext & context);

public:
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_drop_statement();

private:
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_drop_vector_index_statement(TokenLocation location);

private:
    ParserContext & context_;
    ParserSchemaHelper schema_helper_;
};

} // namespace litedb::core::parser
