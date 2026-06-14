#pragma once

#include <memory>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/index/index_manager.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"

namespace litedb::core::planner::access_path
{

/**
 * @brief 访问路径选择器
 */
class AccessPathSelector
{
public:
    explicit AccessPathSelector(const index::IndexManager * index_manager = nullptr) noexcept;

public:
    /**
     * @brief 选择扫描
     * @param database_id 数据库ID
     * @param collection_id 集合ID
     * @param collection_name 集合名称
     * @param predicate 谓词
     * @param location 位置
     * @return 逻辑计划节点
     */
    [[nodiscard]]
    std::unique_ptr<logical::LogicalPlanNode> select_scan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        const binder::bound::BoundExpression * predicate,
        parser::ast::AstNodeLocation location
    ) const;

private:
    const index::IndexManager * index_manager_ {nullptr};  ///< 索引管理器
};

} // namespace litedb::core::planner::access_path
