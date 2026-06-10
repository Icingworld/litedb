#pragma once

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::planner
{

/**
 * @brief 语句计划类型
 */
enum class StatementPlanKind
{
    Use,                ///< 使用数据库
    CreateDatabase,     ///< 创建数据库
    CreateCollection,   ///< 创建集合
    DropDatabase,       ///< 删除数据库
    DropCollection,     ///< 删除集合
    ShowDatabases,      ///< 显示数据库
    ShowCollections,    ///< 显示集合
    DescribeCollection, ///< 描述集合
    Insert,             ///< 插入
    Update,             ///< 更新
    Delete,             ///< 删除
    Query,              ///< 查询
};

/**
 * @brief 语句计划
 */
class StatementPlan
{
public:
    StatementPlan(const StatementPlan &) = delete;

    StatementPlan & operator=(const StatementPlan &) = delete;

    StatementPlan(StatementPlan &&) noexcept = default;

    StatementPlan & operator=(StatementPlan &&) noexcept = default;

    virtual ~StatementPlan() noexcept = default;

protected:
    StatementPlan(StatementPlanKind kind, parser::ast::AstNodeLocation location) noexcept;

public:
    /**
     * @brief 获取语句计划类型
     * @return 语句计划类型
     */
    [[nodiscard]]
    StatementPlanKind kind() const noexcept;

    /**
     * @brief 获取语句计划位置
     * @return 语句计划位置
     */
    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

private:
    StatementPlanKind kind_;                    ///< 语句计划类型
    parser::ast::AstNodeLocation location_;     ///< 语句计划位置
};

} // namespace litedb::core::planner
