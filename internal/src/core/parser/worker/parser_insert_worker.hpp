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

// INSERT 语句解析工作器
class ParserInsertWorker
{
public:
    explicit ParserInsertWorker(ParserContext & context) noexcept;

public:
    // 解析 INSERT 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_insert_statement();

private:
    ParserContext & context_;
};

} // namespace litedb::core::parser
