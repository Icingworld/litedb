#pragma once

#include <string>

#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief ALTER 语句节点
 * @deprecated 暂未实现
 */
class AlterStatement final : public StatementNode
{
public:
    AlterStatement(SchemaObjectType object_type, std::string name, AstNodeLocation location) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 获取对象类型
     * @return 对象类型
     */
    [[nodiscard]]
    SchemaObjectType object_type() const noexcept;

    /**
     * @brief 获取名称
     * @return 名称
     */
    [[nodiscard]]
    const std::string & name() const noexcept;

private:
    SchemaObjectType object_type_;      ///< 对象类型
    std::string name_;                  ///< 名称
};

} // namespace litedb::core::parser::ast
