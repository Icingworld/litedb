#pragma once

#include <cstddef>
#include <expected>
#include <string_view>

#include "core/parser/ast/ast_node.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser
{

class Lexer;

// Parser 共享 Token 游标和诊断上下文
class ParserContext
{
public:
    // 表达式嵌套深度限制保护器
    class ExpressionNestingGuard
    {
        friend class ParserContext;

    public:
        ExpressionNestingGuard() noexcept;

        ExpressionNestingGuard(const ExpressionNestingGuard &) = delete;

        ExpressionNestingGuard & operator=(const ExpressionNestingGuard &) = delete;

        ExpressionNestingGuard(ExpressionNestingGuard && other) noexcept;

        ExpressionNestingGuard & operator=(ExpressionNestingGuard && other) noexcept;

        ~ExpressionNestingGuard();

    private:
        explicit ExpressionNestingGuard(ParserContext & context) noexcept;

    private:
        ParserContext * context_;       // 解析上下文
    };

public:
    explicit ParserContext(Lexer & lexer) noexcept;

public:
    // 初始化上下文并读取第一个 Token
    void initialize();

    // 获取当前 Token
    [[nodiscard]]
    const Token & current() const noexcept;

    // 获取下一个 Token
    [[nodiscard]]
    const Token & peek_next() const noexcept;

    // 获取下下个 Token
    [[nodiscard]]
    const Token & peek_after_next() const noexcept;

    // 移动到下一个 Token
    Token advance();

    // 匹配并消费指定 Token 类型
    bool match(TokenType type);

    // 检查当前 Token 类型
    [[nodiscard]]
    bool check(TokenType type) const;

    // 创建当前 Token 的错误
    [[nodiscard]]
    ParserError make_current_error(
        ParserErrorCode code,
        std::string_view message
    ) const;

    // 创建指定位置和消息的错误
    [[nodiscard]]
    ParserError make_error(
        ParserErrorCode code,
        TokenLocation location,
        std::string_view message
    ) const;

    // 进入表达式嵌套
    [[nodiscard]]
    std::expected<ExpressionNestingGuard, ParserError>
    enter_expression_nesting(TokenLocation location);

    // 检查表达式嵌套深度是否超出限制
    [[nodiscard]]
    bool expression_nesting_limit_reached() const noexcept;

    // 创建表达式嵌套深度超出限制的错误
    [[nodiscard]]
    ParserError make_expression_nesting_error(TokenLocation location) const;

    // 计算表达式嵌套深度
    [[nodiscard]]
    std::expected<std::size_t, ParserError>
    make_expression_parent_depth(
        std::size_t max_child_depth,
        TokenLocation location
    ) const;

    // 消费指定 Token 类型，如果失败则返回指定错误
    [[nodiscard]]
    std::expected<Token, ParserError> consume(
        TokenType type,
        std::string_view message,
        ParserErrorCode code = ParserErrorCode::ExpectedToken
    );

    // 跳过分号
    void skip_semicolon();

    // 获取 AST 节点位置
    [[nodiscard]]
    ast::AstNodeLocation ast_location(TokenLocation location) const noexcept;

private:
    // 离开表达式嵌套
    void leave_expression_nesting() noexcept;

private:
    Lexer & lexer_;                                 // 词法分析器
    Token current_token_;                           // 当前 Token
    Token next_token_;                              // 下一个 Token
    Token next_after_next_token_;                   // 下下个 Token
    std::size_t expression_nesting_depth_;          // 表达式嵌套深度
};

} // namespace litedb::core::parser
