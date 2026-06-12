#pragma once

#include <cstddef>
#include <string_view>

namespace litedb::core::parser
{

/**
 * @brief Token 类型
 */
enum class TokenType
{
    EoF,                ///< 结束标记

    Select,             ///< SELECT
    Create,             ///< CREATE
    Insert,             ///< INSERT
    Delete,             ///< DELETE
    Update,             ///< UPDATE
    Drop,               ///< DROP
    Use,                ///< USE
    Alter,              ///< ALTER
    Show,               ///< SHOW
    Describe,           ///< DESCRIBE
    Desc,               ///< DESC

    Database,           ///< DATABASE
    Collection,         ///< COLLECTION
    Index,              ///< INDEX
    VIndex,             ///< VINDEX
    Databases,          ///< DATABASES
    Collections,        ///< COLLECTIONS
    Indexes,            ///< INDEXES
    VIndexes,           ///< VINDEXES
    Group,              ///< GROUP
    By,                 ///< BY
    Having,             ///< HAVING
    Order,              ///< ORDER
    Asc,                ///< ASC
    Limit,              ///< LIMIT
    Offset,             ///< OFFSET
    In,                 ///< IN
    Between,            ///< BETWEEN
    Like,               ///< LIKE

    Add,                ///< ADD
    Modify,             ///< MODIFY
    Rename,             ///< RENAME
    Column,             ///< COLUMN
    To,                 ///< TO
    Primary,            ///< PRIMARY
    Key,                ///< KEY
    Unique,             ///< UNIQUE
    AutoIncrement,      ///< AUTO_INCREMENT
    Default,            ///< DEFAULT
    Comment,            ///< COMMENT
    Using,              ///< USING
    With,               ///< WITH
    From,               ///< FROM
    Where,              ///< WHERE
    Into,               ///< INTO
    Values,             ///< VALUES
    Set,                ///< SET
    And,                ///< AND
    Or,                 ///< OR
    Not,                ///< NOT
    As,                 ///< AS
    On,                 ///< ON
    If,                 ///< IF
    Exists,             ///< EXISTS
    Is,                 ///< IS
    Null,               ///< NULL
    True,               ///< TRUE
    False,              ///< FALSE

    Integer,            ///< INTEGER
    BigInt,             ///< BIGINT
    Float,              ///< FLOAT
    Double,             ///< DOUBLE
    Varchar,            ///< VARCHAR
    Boolean,            ///< BOOLEAN
    Vector,             ///< VECTOR

    Identifier,         ///< 标识符
    StringLiteral,      ///< 字符串字面量
    IntegerLiteral,     ///< 整数字面量
    FloatLiteral,       ///< 浮点数字面量

    Equal,              ///< =
    NotEqual,           ///< != or <>
    LessThan,           ///< <
    GreaterThan,        ///< >
    LessEqual,          ///< <=
    GreaterEqual,       ///< >=
    Plus,               ///< +
    Minus,              ///< -
    Star,               ///< *
    Slash,              ///< /
    Modulo,             ///< %

    Comma,              ///< ,
    Semicolon,          ///< ;
    Dot,                ///< .
    LeftParen,          ///< (
    RightParen,         ///< )
    LeftBracket,        ///< [
    RightBracket,       ///< ]

    Error               ///< 错误标记
};

/**
 * @brief Token 位置
 */
struct TokenLocation
{
    std::size_t line;       ///< 行号
    std::size_t column;     ///< 列号
};

/**
 * @brief 词法单元
 */
class Token
{
public:
    Token(TokenType type, std::string_view value, std::size_t line, std::size_t column);

    Token(TokenType type, std::string_view value, TokenLocation location);

public:
    /**
     * @brief 获取 Token 类型
     * @return Token 类型
     */
    [[nodiscard]]
    TokenType type() const noexcept;

    /**
     * @brief 获取 Token 值
     * @return Token 值
     */
    [[nodiscard]]
    std::string_view value() const noexcept;

    /**
     * @brief 获取 Token 位置
     * @return Token 位置
     */
    [[nodiscard]]
    TokenLocation location() const noexcept;

private:
    TokenType type_;            ///< Token 类型
    std::string_view value_;    ///< Token 值
    TokenLocation location_;    ///< Token 位置
};

} // namespace litedb::core::parser
