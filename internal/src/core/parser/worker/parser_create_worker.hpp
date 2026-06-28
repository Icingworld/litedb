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
 * @brief CREATE 语句解析工作器
 */
class ParserCreateWorker
{
public:
    explicit ParserCreateWorker(ParserContext & context);

public:
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_create_statement();

private:
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_create_vector_index_statement(TokenLocation location);

private:
    ParserContext & context_;
    ParserSchemaHelper schema_helper_;
};

} // namespace litedb::core::parser
