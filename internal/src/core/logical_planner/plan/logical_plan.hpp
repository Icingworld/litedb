#pragma once

#include <cstdint>

namespace litedb::core::logical_planner::plan
{

/**
 * @brief 逻辑计划类型
 */
enum class LogicalPlanKind : std::uint8_t
{
    // command
    Use = 0,                    ///< 使用数据库
    CreateDatabase = 1,         ///< 创建数据库
    CreateCollection = 2,       ///< 创建集合
    CreateIndex = 3,            ///< 创建索引
    CreateVectorIndex = 4,      ///< 创建向量索引
    DropDatabase = 5,           ///< 删除数据库
    DropCollection = 6,         ///< 删除集合
    DropIndex = 7,              ///< 删除索引
    DropVectorIndex = 8,        ///< 删除向量索引
    ShowDatabases = 9,          ///< 显示数据库
    ShowCollections = 10,       ///< 显示集合
    ShowIndexes = 11,           ///< 显示索引
    ShowVectorIndexes = 12,     ///< 显示向量索引
    DescribeCollection = 13,    ///< 描述集合

    // mutation
    Insert = 14,                ///< 插入
    Update = 15,                ///< 更新
    Delete = 16,                ///< 删除

    // query
    Query = 17,                 ///< 查询
};

/**
 * @brief 逻辑计划
 */
class LogicalPlan
{
public:
    LogicalPlan(const LogicalPlan &) = delete;

    LogicalPlan & operator=(const LogicalPlan &) = delete;

    LogicalPlan(LogicalPlan &&) noexcept = default;

    LogicalPlan & operator=(LogicalPlan &&) noexcept = default;

    virtual ~LogicalPlan() noexcept = default;

protected:
    LogicalPlan(LogicalPlanKind kind) noexcept;

public:
    /**
     * @brief 获取逻辑计划类型
     * @return 逻辑计划类型
     */
    [[nodiscard]]
    LogicalPlanKind kind() const noexcept;

private:
    LogicalPlanKind kind_;                    ///< 逻辑计划类型
};

} // namespace litedb::core::logical_planner::plan
