#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief DROP COLLECTION 语句节点
 * @details 示例：DROP COLLECTION [IF EXISTS] <collection_name>
 */
class DropCollectionStatement final : public StatementNode
{
public:
    DropCollectionStatement(std::string collection_name, bool if_exists, AstNodeLocation location) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(AstNodeVisitor & visitor) const override;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::string collection_name_;   ///< 集合名称
    bool if_exists_;                ///< 是否存在
};

} // namespace litedb::core::parser::ast
