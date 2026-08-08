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

// UPDATE 语句解析工作器
class ParserUpdateWorker
{
public:
    explicit ParserUpdateWorker(ParserContext & context) noexcept;

public:
    // 解析 UPDATE 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_update_statement();

private:
    ParserContext & context_;   // 解析上下文
};

} // namespace litedb::core::parser
