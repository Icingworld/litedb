#pragma once

#include <string>

#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief DROP 语句节点
 * @details 示例：DROP <object_type> <name> [IF EXISTS]
 * @note object_type 可以是 DATABASE 或 COLLECTION
 */
class DropStatement final : public StatementNode
{
public:
    DropStatement(SchemaObjectType object_type, std::string name, bool if_exists, AstNodeLocation location) noexcept;

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

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    SchemaObjectType object_type_;      ///< 对象类型
    std::string name_;                  ///< 名称
    bool if_exists_;                    ///< 是否存在
};

} // namespace litedb::core::parser::ast
