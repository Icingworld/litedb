#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 创建索引方法
 */
enum class CreateIndexMethod
{
    Default,                ///< 默认
    BTree,                  ///< B+ 树
};

/**
 * @brief CREATE INDEX 语句节点
 */
class CreateIndexStatement final : public StatementNode
{
public:
    CreateIndexStatement(
        std::string index_name,
        std::string collection_name,
        std::string column_name,
        bool if_not_exists,
        CreateIndexMethod method,
        AstNodeLocation location
    ) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 获取索引名称
     * @return 索引名称
     */
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 获取列名称
     * @return 列名称
     */
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    /**
     * @brief 是否不存在
     * @return 是否不存在
     */
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    /**
     * @brief 获取创建索引方法
     * @return 创建索引方法
     */
    [[nodiscard]]
    CreateIndexMethod method() const noexcept;

private:
    std::string index_name_;        ///< 索引名称
    std::string collection_name_;   ///< 集合名称
    std::string column_name_;       ///< 列名称
    bool if_not_exists_;            ///< 是否不存在
    CreateIndexMethod method_;      ///< 创建索引方法
};

} // namespace litedb::core::parser::ast
