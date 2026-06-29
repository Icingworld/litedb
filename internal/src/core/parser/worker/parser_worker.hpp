#pragma once

#include <expected>
#include <memory>

#include "core/parser/parser_context.hpp"
#include "core/parser/parser_error.hpp"

namespace litedb::core::parser
{

class Lexer;

namespace ast
{

class StatementNode;

} // namespace ast

/**
 * @brief 解析器主工作器
 */
class ParserWorker
{
public:
    explicit ParserWorker(Lexer & lexer);

public:
    /**
     * @brief 解析 SQL 语句
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse();

private:
    /**
     * @brief 根据首个 Token 按需创建子工作器并分发解析
     * @return 解析结果
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse_statement();

private:
    ParserContext context_;
};

} // namespace litedb::core::parser
