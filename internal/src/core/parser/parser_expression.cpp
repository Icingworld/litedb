#include "core/parser/parser_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"

namespace litedb::core::parser
{

namespace
{

/**
 * @brief 是否为比较运算符
 * @param type  Token 类型
 * @return 是否为比较运算符
 */
[[nodiscard]]
bool is_comparison_operator(TokenType type) noexcept
{
    return type == TokenType::Equal
        || type == TokenType::NotEqual
        || type == TokenType::LessThan
        || type == TokenType::LessEqual
        || type == TokenType::GreaterThan
        || type == TokenType::GreaterEqual;
}

/**
 * @brief 是否为字面量 Token 
 * @param type  Token 类型
 * @return 是否为字面量 Token 
 */
[[nodiscard]]
bool is_literal_token(TokenType type) noexcept
{
    return type == TokenType::IntegerLiteral
        || type == TokenType::FloatLiteral
        || type == TokenType::StringLiteral
        || type == TokenType::True
        || type == TokenType::False
        || type == TokenType::Null;
}

} // namespace

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_expression()
{
    // 递归下降解析表达式
    // 完整的解析流程为：
    // parse_expression()
    //   └─> parse_or_expression()                          // OR，最低优先级逻辑运算符
    //         ├─> parse_and_expression()                   // 左侧；循环中遇到 OR 时解析右侧
    //         │     ├─> parse_not_expression()             // 左侧；循环中遇到 AND 时解析右侧
    //         │     │     ├─> [NOT] parse_not_expression() // 一元 NOT，右递归
    //         │     │     └─> parse_comparison_expression()
    //         │     │           ├─> parse_additive_expression()                              // 比较左操作数
    //         │     │           ├─> [==, !=, <, <=, >, >=] parse_additive_expression()       // 二元比较（仅一次）
    //         │     │           ├─> [NOT] LIKE parse_additive_expression()                   // 模式匹配，NOT 可选
    //         │     │           ├─> [NOT] IN ( parse_expression(), ... )                     // 集合成员，NOT 可选
    //         │     │           ├─> [NOT] BETWEEN ... AND ...                                // 范围，NOT 可选
    //         │     │           └─> 无后缀比较/匹配操作符时，直接返回左侧
    //         │     │                 └─> parse_additive_expression()                        // + / -
    //         │     │                       ├─> parse_multiplicative_expression()            // * / %
    //         │     │                       │     ├─> parse_unary_expression()               // 一元 + / -
    //         │     │                       │     │     ├─> [+, -] parse_unary_expression()  // 右递归
    //         │     │                       │     │     └─> parse_primary_expression()
    //         │     │                       │     │           ├─> parse_literal_expression()           // 字面量
    //         │     │                       │     │           ├─> parse_column_reference_expression()  // 列引用（支持 表.列）
    //         │     │                       │     │           ├─> parse_vector_expression()            // 向量字面量 [...]
    //         │     │                       │     │           └─> ( parse_expression() )               // 括号分组，回到顶层
    //         │     │                       │     └─> 循环：* / % 与右侧 parse_unary_expression()
    //         │     │                       └─> 循环：+ / - 与右侧 parse_multiplicative_expression()
    //         │     └─> 循环：AND 与右侧 parse_not_expression()
    //         └─> 循环：OR 与右侧 parse_and_expression()

    return parse_or_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_or_expression()
{
    // 解析左侧 AND 表达式
    auto left = parse_and_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理 OR 表达式
    while (check(TokenType::Or)) {
        // 消耗 OR 关键字
        const Token op = advance();

        // 解析右侧 AND 表达式
        auto right = parse_and_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_and_expression()
{
    // 解析左侧 NOT 表达式
    auto left = parse_not_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理 AND 表达式
    while (check(TokenType::And)) {
        // 消耗 AND 关键字
        const Token op = advance();

        // 解析右侧 NOT 表达式
        auto right = parse_not_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_not_expression()
{
    // 尝试匹配 NOT 关键字
    if (check(TokenType::Not)) {
        // 消耗 NOT 关键字
        const Token op = advance();

        // 继续解析 NOT 表达式
        auto operand = parse_not_expression();
        if (!operand.has_value()) [[unlikely]] {
            return std::unexpected(operand.error());
        }

        return std::make_unique<ast::UnaryExpression>(
            op.type(),
            std::move(operand.value()),
            ast_location(op.location())
        );
    }

    return parse_comparison_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_comparison_expression()
{
    // 解析左侧加法表达式
    auto left = parse_additive_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 尝试匹配比较运算符
    if (is_comparison_operator(current_token_.type())) {
        // 消耗比较运算符
        const Token op = advance();

        // 解析右侧加法表达式
        auto right = parse_additive_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        return std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    // 尝试匹配 NOT 关键字
    bool negated = false;
    TokenLocation not_location = current_token_.location();
    if (check(TokenType::Not)) {
        // 设置 NOT 标志
        negated = true;
        not_location = advance().location();
    }

    // 尝试匹配 LIKE 关键字
    if (check(TokenType::Like)) {
        // 消耗 LIKE 关键字
        const Token op = advance();

        // 解析右侧加法表达式
        auto pattern = parse_additive_expression();
        if (!pattern.has_value()) [[unlikely]] {
            return std::unexpected(pattern.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::LikeExpression>(
            std::move(left.value()),
            std::move(pattern.value()),
            ast_location(op.location())
        );
        // 如果设置了 NOT 标志，实际上创建一个一元表达式
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    // 尝试匹配 IN 关键字
    if (check(TokenType::In)) {
        // 消耗 IN 关键字
        const Token op = advance();

        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after IN");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 检查是否为空列表
        if (check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one IN value"));
        }

        // 解析值列表
        ast::InExpression::ValueList values;
        // 循环解析值
        while (true) {
            // 解析表达式
            auto value = parse_expression();
            if (!value.has_value()) [[unlikely]] {
                return std::unexpected(value.error());
            }
            values.push_back(std::move(value.value()));

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after IN values");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::InExpression>(
            std::move(left.value()),
            std::move(values),
            ast_location(op.location())
        );
        // 检测 NOT 标志
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    // 尝试匹配 BETWEEN 关键字
    if (check(TokenType::Between)) {
        // 消耗 BETWEEN 关键字
        const Token op = advance();

        // 解析下界表达式
        auto lower = parse_additive_expression();
        if (!lower.has_value()) [[unlikely]] {
            return std::unexpected(lower.error());
        }

        // 期望 AND
        auto and_token = consume(TokenType::And, "Expected AND in BETWEEN expression");
        if (!and_token.has_value()) [[unlikely]] {
            return std::unexpected(and_token.error());
        }

        // 解析上界表达式
        auto upper = parse_additive_expression();
        if (!upper.has_value()) [[unlikely]] {
            return std::unexpected(upper.error());
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::BetweenExpression>(
            std::move(left.value()),
            std::move(lower.value()),
            std::move(upper.value()),
            ast_location(op.location())
        );
        // 检测 NOT 标志
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                ast_location(not_location)
            );
        }
        return expression;
    }

    // 不符合任何比较/匹配操作符，且设置了 NOT 标志，则错误
    if (negated) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::ExpectedToken, "Expected LIKE, IN, or BETWEEN after NOT"));
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_additive_expression()
{
    // 解析左侧乘法表达式
    auto left = parse_multiplicative_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理加法表达式
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        // 消耗加法运算符
        const Token op = advance();

        // 解析右侧乘法表达式
        auto right = parse_multiplicative_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_multiplicative_expression()
{
    // 解析左侧一元表达式
    auto left = parse_unary_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(left.error());
    }

    // 左结合循环处理乘法表达式
    while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Modulo)) {
        // 消耗乘法运算符
        const Token op = advance();

        // 解析右侧一元表达式
        auto right = parse_unary_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(right.error());
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(left.value()),
            op.type(),
            std::move(right.value()),
            ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_unary_expression()
{
    // 尝试匹配 + 或 - 关键字
    if (check(TokenType::Plus) || check(TokenType::Minus)) {
        // 消耗 + 或 - 关键字
        const Token op = advance();

        // 解析右侧一元表达式
        auto operand = parse_unary_expression();
        if (!operand.has_value()) [[unlikely]] {
            return std::unexpected(operand.error());
        }

        return std::make_unique<ast::UnaryExpression>(
            op.type(),
            std::move(operand.value()),
            ast_location(op.location())
        );
    }

    return parse_primary_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_primary_expression()
{
    // 根据不同的 Token 类型，解析不同的表达式

    // 尝试匹配字面量
    if (is_literal_token(current_token_.type())) {
        return parse_literal_expression();
    }
    // 尝试匹配列引用
    if (check(TokenType::Identifier)) {
        return parse_function_call_or_column_reference();
    }
    // 遇到 [ 则尝试匹配向量字面量
    if (check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }
    // 遇到 ( 则尝试匹配括号分组
    if (match(TokenType::LeftParen)) {
        // 解析括号内的表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after expression");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        return expression;
    }

    return std::unexpected(make_current_error(ParserErrorCode::ExpectedExpression, "Expected expression"));
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_column_reference_expression()
{
    // 期望标识符
    auto first = consume(TokenType::Identifier, "Expected column name");
    if (!first.has_value()) [[unlikely]] {
        return std::unexpected(first.error());
    }

    // 列名格式为 column 或 qualifier.column
    std::optional<std::string> qualifier;
    std::string column(first->value());

    // 尝试匹配 .
    if (match(TokenType::Dot)) {
        // 如果存在 . 则列名为 qualifier.column
        qualifier = std::move(column);

        // 期望标识符
        auto second = consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) [[unlikely]] {
            return std::unexpected(second.error());
        }
        // 更新列名
        column = std::string(second->value());
    }

    // 创建列引用表达式
    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(column),
        ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_function_call_or_column_reference()
{
    auto first = consume(TokenType::Identifier, "Expected function or column name");
    if (!first.has_value()) [[unlikely]] {
        return std::unexpected(first.error());
    }

    std::string name(first->value());
    if (match(TokenType::LeftParen)) {
        ast::FunctionCallExpression::ArgumentList arguments;
        if (!check(TokenType::RightParen)) {
            while (true) {
                auto argument = parse_expression();
                if (!argument.has_value()) [[unlikely]] {
                    return std::unexpected(argument.error());
                }
                arguments.push_back(std::move(argument.value()));
                if (!match(TokenType::Comma)) {
                    break;
                }
            }
        }

        auto right_paren = consume(TokenType::RightParen, "Expected ')' after function arguments");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        return std::make_unique<ast::FunctionCallExpression>(
            std::move(name),
            std::move(arguments),
            ast_location(first->location())
        );
    }

    std::optional<std::string> qualifier;
    if (match(TokenType::Dot)) {
        qualifier = std::move(name);
        auto second = consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) [[unlikely]] {
            return std::unexpected(second.error());
        }
        name = std::string(second->value());
    }

    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(name),
        ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_wildcard_or_column_reference()
{
    // 尝试匹配 *
    if (check(TokenType::Star)) {
        // 消耗 *
        const Token star = advance();

        // 创建无限定符的通配符表达式
        return std::make_unique<ast::WildcardExpression>(ast_location(star.location()));
    }

    // 尝试匹配列引用
    auto first = consume(TokenType::Identifier, "Expected select item");
    if (!first.has_value()) [[unlikely]] {
        return std::unexpected(first.error());
    }

    // 列名格式为 qualifier.column
    std::optional<std::string> qualifier;
    std::string column(first->value());
    if (match(TokenType::Dot)) {
        qualifier = std::move(column);

        // 尝试匹配 *
        if (check(TokenType::Star)) {
            // 消耗 *
            advance();

            // 创建带限定符的通配符表达式
            return std::make_unique<ast::WildcardExpression>(
                std::move(qualifier),
                ast_location(first->location())
            );
        }

        // 期望标识符
        auto second = consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) [[unlikely]] {
            return std::unexpected(second.error());
        }
        column = std::string(second->value());
    } else if (match(TokenType::LeftParen)) {
        ast::FunctionCallExpression::ArgumentList arguments;
        if (!check(TokenType::RightParen)) {
            while (true) {
                auto argument = parse_expression();
                if (!argument.has_value()) [[unlikely]] {
                    return std::unexpected(argument.error());
                }
                arguments.push_back(std::move(argument.value()));
                if (!match(TokenType::Comma)) {
                    break;
                }
            }
        }

        auto right_paren = consume(TokenType::RightParen, "Expected ')' after function arguments");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        return std::make_unique<ast::FunctionCallExpression>(
            std::move(column),
            std::move(arguments),
            ast_location(first->location())
        );
    }

    // 创建普通的列引用表达式
    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(column),
        ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_literal_expression()
{
    // 如果是 [ 则尝试匹配向量字面量
    if (check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }

    // 如果不是字面量 Token，则错误
    if (!is_literal_token(current_token_.type())) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::ExpectedLiteral, "Expected literal"));
    }

    const Token token = advance();
    return std::make_unique<ast::LiteralExpression>(
        token.type(),
        std::string(token.value()),
        ast_location(token.location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError> ParserWorker::parse_vector_expression()
{
    // 消耗 [
    const Token left_bracket = advance();
    // 检查是否空列表
    if (check(TokenType::RightBracket)) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one vector element"));
    }

    // 解析元素列表
    ast::VectorExpression::ElementList elements;
    // 循环解析元素
    while (true) {
        // 解析表达式
        auto element = parse_expression();
        if (!element.has_value()) [[unlikely]] {
            return std::unexpected(element.error());
        }
        elements.push_back(std::move(element.value()));

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 期望 ]
    auto right_bracket = consume(TokenType::RightBracket, "Expected ']' after vector literal");
    if (!right_bracket.has_value()) [[unlikely]] {
        return std::unexpected(right_bracket.error());
    }

    return std::make_unique<ast::VectorExpression>(
        std::move(elements),
        ast_location(left_bracket.location())
    );
}

} // namespace litedb::core::parser
