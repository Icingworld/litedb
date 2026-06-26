#include "core/parser/parser_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/select_statement.hpp"

namespace litedb::core::parser
{

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_select_statement()
{
    // 保存并消耗 SELECT 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析选择列表
    ast::SelectStatement::SelectList select_list;
    while (true) {
        // 解析选择项，* 或列引用
        auto item = parse_wildcard_or_column_reference();
        if (!item.has_value()) [[unlikely]] {
            return std::unexpected(item.error());
        }
        select_list.push_back(std::move(item.value()));

        // 列表元素之间期望使用逗号分隔
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    // 期望 FROM 关键字
    auto from = consume(TokenType::From, "Expected FROM after select list");
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(from.error());
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    // 尝试匹配 WHERE 关键字
    std::unique_ptr<ast::ExpressionNode> where;
    if (match(TokenType::Where)) {
        // 解析表达式
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    // 解析排序列表
    ast::SelectStatement::OrderByList order_by;
    // 尝试匹配 ORDER BY 关键字
    if (match(TokenType::Order)) {
        auto by = consume(TokenType::By, "Expected BY after ORDER");
        if (!by.has_value()) [[unlikely]] {
            return std::unexpected(by.error());
        }

        // 解析排序项列表
        while (true) {
            // 解析表达式
            auto expression = parse_expression();
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(expression.error());
            }

            // 默认为升序排序
            bool ascending = true;
            if (match(TokenType::Asc)) {
                ascending = true;
            } else if (match(TokenType::Desc)) {
                ascending = false;
            }

            order_by.push_back(ast::OrderByItem {std::move(expression.value()), ascending});

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }
    }

    // 尝试匹配 LIMIT 关键字
    std::optional<std::size_t> limit;
    if (match(TokenType::Limit)) {
        // 解析整数
        auto value = parse_integer_value("Expected LIMIT value");
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        limit = value.value();
    }

    // 尝试匹配 OFFSET 关键字
    std::optional<std::size_t> offset;
    if (match(TokenType::Offset)) {
        // 解析整数
        auto value = parse_integer_value("Expected OFFSET value");
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        offset = value.value();
    }

    return std::make_unique<ast::SelectStatement>(
        std::move(select_list),
        std::move(collection.value()),
        std::move(where),
        std::move(order_by),
        limit,
        offset,
        ast_location(location)
    );
}

} // namespace litedb::core::parser
