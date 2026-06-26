#pragma once

#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief SHOW 语句节点
 * @details 示例：SHOW <object_type>
 * @note object_type 可以是 DATABASE 或 COLLECTION
 */
class ShowStatement final : public StatementNode
{
public:
    ShowStatement(SchemaObjectType object_type, AstNodeLocation location) noexcept;

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
     * @brief 获取对象类型
     * @return 对象类型
     */
    [[nodiscard]]
    SchemaObjectType object_type() const noexcept;

private:
    SchemaObjectType object_type_;      ///< 对象类型
};

} // namespace litedb::core::parser::ast
