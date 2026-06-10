#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/planner/logical/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 逻辑扫描节点
 */
class LogicalScan final : public LogicalPlanNode
{
public:
    LogicalScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合ID
     * @return 集合ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    common::DatabaseId database_id_;                 ///< 数据库ID
    common::CollectionId collection_id_;             ///< 集合ID
    std::string collection_name_;                    ///< 集合名称
};

} // namespace litedb::core::planner::logical
