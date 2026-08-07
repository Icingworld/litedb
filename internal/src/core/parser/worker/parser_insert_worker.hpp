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
 * @brief INSERT 语句解析工作器
 */
class ParserInsertWorker
{
public:
    explicit ParserInsertWorker(ParserContext & context);

public:
    /**
     * @brief 解析 INSERT 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
    parse_insert_statement();

private:
    ParserContext & context_;   // 解析上下文
};

} // namespace litedb::core::parser
