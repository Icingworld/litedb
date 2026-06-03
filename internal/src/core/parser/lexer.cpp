#include "core/parser/lexer.hpp"

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace litedb::core::parser
{

namespace
{

// 编译器常量表，用于快速查找获取词法单元类型
constexpr auto KEYWORDS = std::to_array<std::pair<std::string_view, TokenType>>({
    {"SELECT", TokenType::Select},
    {"CREATE", TokenType::Create},
    {"INSERT", TokenType::Insert},
    {"DELETE", TokenType::Delete},
    {"UPDATE", TokenType::Update},
    {"DROP", TokenType::Drop},
    {"USE", TokenType::Use},
    {"ALTER", TokenType::Alter},
    {"SHOW", TokenType::Show},
    {"DESCRIBE", TokenType::Describe},
    {"DESC", TokenType::Desc},
    {"DATABASE", TokenType::Database},
    {"COLLECTION", TokenType::Collection},
    {"INDEX", TokenType::Index},
    {"VINDEX", TokenType::VIndex},
    {"DATABASES", TokenType::Databases},
    {"COLLECTIONS", TokenType::Collections},
    {"INDEXES", TokenType::Indexes},
    {"VINDEXES", TokenType::VIndexes},
    {"GROUP", TokenType::Group},
    {"BY", TokenType::By},
    {"HAVING", TokenType::Having},
    {"ORDER", TokenType::Order},
    {"ASC", TokenType::Asc},
    {"LIMIT", TokenType::Limit},
    {"OFFSET", TokenType::Offset},
    {"IN", TokenType::In},
    {"BETWEEN", TokenType::Between},
    {"LIKE", TokenType::Like},
    {"ADD", TokenType::Add},
    {"MODIFY", TokenType::Modify},
    {"RENAME", TokenType::Rename},
    {"COLUMN", TokenType::Column},
    {"TO", TokenType::To},
    {"PRIMARY", TokenType::Primary},
    {"KEY", TokenType::Key},
    {"UNIQUE", TokenType::Unique},
    {"AUTO_INCREMENT", TokenType::AutoIncrement},
    {"DEFAULT", TokenType::Default},
    {"COMMENT", TokenType::Comment},
    {"USING", TokenType::Using},
    {"WITH", TokenType::With},
    {"FROM", TokenType::From},
    {"WHERE", TokenType::Where},
    {"INTO", TokenType::Into},
    {"VALUES", TokenType::Values},
    {"SET", TokenType::Set},
    {"AND", TokenType::And},
    {"OR", TokenType::Or},
    {"NOT", TokenType::Not},
    {"AS", TokenType::As},
    {"ON", TokenType::On},
    {"IF", TokenType::If},
    {"EXISTS", TokenType::Exists},
    {"IS", TokenType::Is},
    {"NULL", TokenType::Null},
    {"TRUE", TokenType::True},
    {"FALSE", TokenType::False},
});

/**
 * @brief 获取关键字的词法单元类型
 * @param value 关键字
 * @return 关键字的词法单元类型
 */
[[nodiscard]]
constexpr std::optional<TokenType> keyword_type(std::string_view value) noexcept
{
    for (const auto & [keyword, type] : KEYWORDS) {
        if (keyword == value) {
            return type;
        }
    }

    return std::nullopt;
}

} // namespace

Lexer::Lexer(std::string input)
    : input_(std::move(input))
    , position_(0)
    , location_({1, 1})
    , peeked_token_(std::nullopt)
{
}

Token Lexer::next()
{
    // 如果已经预读，直接返回预读的词法单元
    if (peeked_token_.has_value()) {
        const Token token = peeked_token_.value();
        peeked_token_ = std::nullopt;
        return token;
    }

    // 没有预读，尝试获取下一个词法单元
    return next_internal();
}

const Token & Lexer::peek()
{
    if (!peeked_token_.has_value()) {
        peeked_token_ = next_internal();
    }

    return peeked_token_.value();
}

bool Lexer::has_more() const noexcept
{
    return peeked_token_.has_value() || position_ < input_.length();
}

TokenLocation Lexer::location() const noexcept
{
    return location_;
}

Token Lexer::next_internal()
{
    // 跳过空白字符
    skip_whitespace();

    // 如果已经到达输入末尾，返回结束标记
    if (position_ >= input_.length()) {
        return Token(TokenType::EoF, "", location_);
    }
}

void Lexer::skip_whitespace()
{
    while (position_ < input_.length()) {
        const char c = advance();
        if (!std::isspace(c)) {
            break;
        }
    }
}

} // namespace litedb::core::parser
