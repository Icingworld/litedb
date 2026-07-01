#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief DROP VINDEX 语句计划
 */
class DropVectorIndexPlan final : public StatementPlan
{
public:
    DropVectorIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::string index_name,
        bool if_exists,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 获取索引名称
     * @return 索引名称
     */
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    common::DatabaseId database_id_;                    ///< 数据库 ID
    common::CollectionId collection_id_;                ///< 集合 ID
    std::string collection_name_;                       ///< 集合名称
    std::string index_name_;                            ///< 索引名称
    bool if_exists_;                                    ///< 是否存在
};

} // namespace litedb::core::planner::plan
