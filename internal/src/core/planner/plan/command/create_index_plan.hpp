#pragma once

#include <string>

#include "core/catalog/catalog_entry.hpp"
#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief CREATE INDEX ????
 */
class CreateIndexPlan final : public StatementPlan
{
public:
    CreateIndexPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::ColumnId column_id,
        std::string column_name,
        std::string index_name,
        catalog::CatalogIndexKind index_kind,
        bool unique,
        bool if_not_exists,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief ????? ID
     * @return ??? ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief ???? ID
     * @return ?? ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief ??????
     * @return ????
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief ??? ID
     * @return ? ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief ?????
     * @return ???
     */
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    /**
     * @brief ??????
     * @return ????
     */
    [[nodiscard]]
    const std::string & index_name() const noexcept;

    /**
     * @brief ??????
     * @return ????
     */
    [[nodiscard]]
    catalog::CatalogIndexKind index_kind() const noexcept;

    /**
     * @brief ????
     * @return ????
     */
    [[nodiscard]]
    bool unique() const noexcept;

    /**
     * @brief ????
     * @return ????
     */
    [[nodiscard]]
    bool if_not_exists() const noexcept;

private:
    common::DatabaseId database_id_;            ///< ??? ID
    common::CollectionId collection_id_;        ///< ?? ID
    std::string collection_name_;               ///< ????
    common::ColumnId column_id_;                ///< ? ID
    std::string column_name_;                   ///< ???
    std::string index_name_;                    ///< ????
    catalog::CatalogIndexKind index_kind_;      ///< ????
    bool unique_;                               ///< ????
    bool if_not_exists_;                        ///< ????
};

} // namespace litedb::core::planner::plan
