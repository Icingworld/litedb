#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/planner/statement/statement_plan.hpp"

namespace litedb::core::planner
{

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
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    const std::string & index_name() const noexcept;

    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::string index_name_;
    bool if_exists_;
};

} // namespace litedb::core::planner
