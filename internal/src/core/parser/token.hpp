#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace litedb::core::parser
{

/**
 * @brief 词法单元类型
 * @note 当词法单元数量超过 256 时，需要更换存储类型
 */
enum class TokenType : std::uint8_t
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
 * @brief 词法单元位置
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
     * @brief 获取词法单元类型
     * @return 词法单元类型
     */
    [[nodiscard]]
    TokenType type() const noexcept;

    /**
     * @brief 获取词法单元值
     * @return 词法单元值
     */
    [[nodiscard]]
    std::string_view value() const noexcept;

    /**
     * @brief 获取词法单元位置
     * @return 词法单元位置
     */
    [[nodiscard]]
    TokenLocation location() const noexcept;

private:
    TokenType type_;            ///< 词法单元类型
    std::string_view value_;    ///< 词法单元值
    TokenLocation location_;    ///< 词法单元位置
};

} // namespace litedb::core::parser
