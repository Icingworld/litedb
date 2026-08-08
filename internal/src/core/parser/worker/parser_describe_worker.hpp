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

// DESCRIBE 语句解析工作器
class ParserDescribeWorker
{
public:
    explicit ParserDescribeWorker(ParserContext & context) noexcept;

public:
    // 解析 DESCRIBE COLLECTION 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_describe_collection_statement();

private:
    ParserContext & context_;
};

} // namespace litedb::core::parser
