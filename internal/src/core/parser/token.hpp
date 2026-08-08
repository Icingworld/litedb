#pragma once

#include <cstddef>
#include <string_view>
#include <optional>
#include <array>

namespace litedb::core::parser
{

// Token 类型
enum class TokenType
{
    EoF,                // 结束标记

    Select,
    Create,
    Insert,
    Delete,
    Update,
    Drop,
    Use,
    Show,
    Describe,
    Desc,

    Database,
    Collection,
    Index,
    VIndex,
    Databases,
    Collections,
    Indexes,
    VIndexes,
    By,
    Order,
    Asc,
    Limit,
    Offset,
    In,
    Between,
    Like,

    Unique,
    Default,
    Comment,
    Using,
    BTree,
    With,
    From,
    Where,
    Into,
    Values,
    Set,
    And,
    Or,
    Not,
    As,
    On,
    If,
    Exists,
    Null,
    True,
    False,

    Integer,
    BigInt,
    Float,
    Double,
    Varchar,
    Boolean,
    Vector,

    Identifier,         // 标识符
    StringLiteral,      // 字符串字面量
    IntegerLiteral,     // 整数字面量
    FloatLiteral,       // 浮点数字面量

    Equal,              // =
    NotEqual,           // != or <>
    LessThan,           // <
    GreaterThan,        // >
    LessEqual,          // <=
    GreaterEqual,       // >=
    Plus,               // +
    Minus,              // -
    Star,               // *
    Slash,              // /
    Modulo,             // %

    Comma,              // ,
    Semicolon,          // ;
    Dot,                // .
    LeftParen,          // (
    RightParen,         // )
    LeftBracket,        // [
    RightBracket,       // ]

    Error,              // 错误标记
};

// Token 位置
struct TokenLocation
{
    std::size_t line;       // 行号
    std::size_t column;     // 列号
};

// 词法单元
class Token
{
public:
    Token(TokenType type, std::string_view value, std::size_t line, std::size_t column);

    Token(TokenType type, std::string_view value, TokenLocation location);

public:
    // 获取 Token 类型
    [[nodiscard]]
    TokenType type() const noexcept;

    // 获取 Token 值
    [[nodiscard]]
    std::string_view value() const noexcept;

    // 获取 Token 位置
    [[nodiscard]]
    TokenLocation location() const noexcept;

private:
    TokenType type_;
    std::string_view value_;
    TokenLocation location_;
};

// 编译期常量表，用于快速查找获取 Token 类型
inline constexpr auto TOKEN_KEYWORDS_TABLE = std::to_array<std::pair<std::string_view, TokenType>>({
    {"SELECT", TokenType::Select},
    {"CREATE", TokenType::Create},
    {"INSERT", TokenType::Insert},
    {"DELETE", TokenType::Delete},
    {"UPDATE", TokenType::Update},
    {"DROP", TokenType::Drop},
    {"USE", TokenType::Use},
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
    {"BY", TokenType::By},
    {"ORDER", TokenType::Order},
    {"ASC", TokenType::Asc},
    {"LIMIT", TokenType::Limit},
    {"OFFSET", TokenType::Offset},
    {"IN", TokenType::In},
    {"BETWEEN", TokenType::Between},
    {"LIKE", TokenType::Like},
    {"UNIQUE", TokenType::Unique},
    {"DEFAULT", TokenType::Default},
    {"COMMENT", TokenType::Comment},
    {"USING", TokenType::Using},
    {"BTREE", TokenType::BTree},
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
    {"NULL", TokenType::Null},
    {"TRUE", TokenType::True},
    {"FALSE", TokenType::False},
    {"INTEGER", TokenType::Integer},
    {"BIGINT", TokenType::BigInt},
    {"FLOAT", TokenType::Float},
    {"DOUBLE", TokenType::Double},
    {"VARCHAR", TokenType::Varchar},
    {"BOOLEAN", TokenType::Boolean},
    {"VECTOR", TokenType::Vector},
});

// 是否为比较运算符
[[nodiscard]]
bool is_comparison_operator(TokenType type) noexcept;

// 是否为字面量 Token
[[nodiscard]]
bool is_literal_token(TokenType type) noexcept;

// 获取关键字的 Token 类型
[[nodiscard]]
inline constexpr std::optional<TokenType> keyword_type(std::string_view value) noexcept
{
    for (const auto & [keyword, type] : TOKEN_KEYWORDS_TABLE) {
        if (keyword == value) {
            return type;
        }
    }

    return std::nullopt;
}

} // namespace litedb::core::parser
