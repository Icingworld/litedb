#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/common/logical_type.hpp"
#include "core/logical_plan/node/logical_filter.hpp"
#include "core/logical_plan/node/logical_index_scan.hpp"
#include "core/logical_plan/node/logical_limit.hpp"
#include "core/logical_plan/node/logical_order_by.hpp"
#include "core/logical_plan/node/logical_projection.hpp"
#include "core/logical_plan/node/logical_scan.hpp"
#include "core/physical_plan/node/physical_filter.hpp"
#include "core/physical_plan/node/physical_index_scan.hpp"
#include "core/physical_plan/node/physical_limit.hpp"
#include "core/physical_plan/node/physical_projection.hpp"
#include "core/physical_plan/node/physical_seq_scan.hpp"
#include "core/physical_plan/node/physical_sort.hpp"
#include "core/physical_plan/physical_planner.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;
using namespace litedb::core::catalog;
using namespace litedb::core::common;
using namespace litedb::core::parser::ast;
using namespace litedb::core::physical_plan;
using namespace litedb::core::planner::logical;

constexpr AstNodeLocation loc {1, 1};

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LogicalType type(LogicalTypeId id)
{
    return LogicalType {.id = id, .parameter = std::nullopt};
}

std::unique_ptr<BoundLiteralExpression> literal(LogicalTypeId id, std::string value)
{
    return std::make_unique<BoundLiteralExpression>(type(id), std::move(value), loc);
}

void test_lower_unary_chain()
{
    std::vector<BoundProjectionItem> projections;
    projections.push_back(BoundProjectionItem {
        .expression = literal(LogicalTypeId::BigInt, "1"),
        .alias = "id",
    });

    std::vector<BoundOrderByItem> order_by;
    order_by.push_back(BoundOrderByItem {
        .expression = literal(LogicalTypeId::BigInt, "1"),
        .ascending = false,
    });

    auto logical = std::make_unique<LogicalLimit>(
        std::make_unique<LogicalOrderBy>(
            std::make_unique<LogicalProjection>(
                std::make_unique<LogicalFilter>(
                    std::make_unique<LogicalScan>(DatabaseId {1}, CollectionId {2}, "users", loc),
                    literal(LogicalTypeId::Boolean, "true"),
                    loc
                ),
                std::move(projections),
                loc
            ),
            std::move(order_by),
            loc
        ),
        10,
        5,
        loc
    );

    PhysicalPlanner planner;
    auto physical = planner.plan(*logical);
    require(physical->kind() == PhysicalPlanNodeKind::Limit, "root should lower to physical limit");

    const auto & limit = static_cast<const PhysicalLimit &>(*physical);
    require(limit.limit().value() == 10, "limit value mismatch");
    require(limit.offset().value() == 5, "offset value mismatch");
    require(limit.child().kind() == PhysicalPlanNodeKind::Sort, "order by should lower to sort");

    const auto & sort = static_cast<const PhysicalSort &>(limit.child());
    require(sort.order_by().size() == 1, "sort key count mismatch");
    require(!sort.order_by()[0].ascending, "sort direction mismatch");
    require(sort.child().kind() == PhysicalPlanNodeKind::Projection, "projection should be below sort");

    const auto & projection = static_cast<const PhysicalProjection &>(sort.child());
    require(projection.projections().size() == 1, "projection count mismatch");
    require(projection.child().kind() == PhysicalPlanNodeKind::Filter, "filter should be below projection");

    const auto & filter = static_cast<const PhysicalFilter &>(projection.child());
    require(filter.predicate().type().id == LogicalTypeId::Boolean, "filter predicate type mismatch");
    require(filter.child().kind() == PhysicalPlanNodeKind::SeqScan, "scan should lower to seq scan");

    const auto & scan = static_cast<const PhysicalSeqScan &>(filter.child());
    require(scan.database_id() == DatabaseId {1}, "seq scan database id mismatch");
    require(scan.collection_id() == CollectionId {2}, "seq scan collection id mismatch");
    require(scan.collection_name() == "users", "seq scan collection name mismatch");
}

void test_lower_index_scan()
{
    LogicalIndexLookup lookup;
    lookup.kind = LogicalIndexLookupKind::Range;

    LogicalIndexScan logical {
        DatabaseId {1},
        CollectionId {2},
        "users",
        IndexId {3},
        "idx_age",
        CatalogIndexKind::BTree,
        ColumnId {4},
        "age",
        lookup,
        loc,
    };

    PhysicalPlanner planner;
    auto physical = planner.plan(logical);
    require(physical->kind() == PhysicalPlanNodeKind::IndexScan, "index scan should lower to physical index scan");

    const auto & scan = static_cast<const PhysicalIndexScan &>(*physical);
    require(scan.database_id() == DatabaseId {1}, "index scan database id mismatch");
    require(scan.collection_id() == CollectionId {2}, "index scan collection id mismatch");
    require(scan.collection_name() == "users", "index scan collection name mismatch");
    require(scan.index_id() == IndexId {3}, "index scan id mismatch");
    require(scan.index_name() == "idx_age", "index scan name mismatch");
    require(scan.index_kind() == CatalogIndexKind::BTree, "index kind mismatch");
    require(scan.column_id() == ColumnId {4}, "index scan column id mismatch");
    require(scan.column_name() == "age", "index scan column name mismatch");
    require(scan.lookup().kind == PhysicalIndexLookupKind::Range, "index lookup kind mismatch");
}

} // namespace

int main()
{
    try {
        test_lower_unary_chain();
        test_lower_index_scan();
    } catch (const std::exception & e) {
        std::cerr << "physical_plan_tests failed: " << e.what() << '\n';
        return 1;
    }

    std::cout << "physical_plan_tests passed\n";
    return 0;
}
