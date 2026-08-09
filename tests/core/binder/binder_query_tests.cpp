#include "test_support.hpp"

#include <exception>
#include <iostream>

#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"

namespace
{

using namespace litedb::test::binder;

void test_projection_names_and_wildcard_order()
{
    Fixture fixture;
    auto select = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT id, age + 1, name AS display_name FROM users;"
    );
    require(select->collection_id() == fixture.users_id, "SELECT collection id mismatch");
    require(select->projections().size() == 3, "projection count mismatch");
    require(select->projections()[0].output_name == "id", "column output name mismatch");
    require(select->projections()[1].output_name == "expr2", "generated output name mismatch");
    require(select->projections()[2].output_name == "display_name", "alias output name mismatch");

    auto wildcard = bind_ok<BoundSelectStatement>(fixture, "SELECT * FROM users;");
    require(wildcard->projections().size() == 4, "wildcard expansion count mismatch");
    const char * expected_names[] = {"id", "name", "age", "embedding"};
    const ColumnId expected_ids[] = {
        fixture.id_column_id,
        fixture.name_column_id,
        fixture.age_column_id,
        fixture.embedding_column_id,
    };
    for (std::size_t index = 0; index < wildcard->projections().size(); ++index) {
        const auto & item = wildcard->projections()[index];
        require(item.output_name == expected_names[index], "wildcard output order mismatch");
        const auto & column = static_cast<const BoundColumnRefExpression &>(*item.expression);
        require(column.column_id() == expected_ids[index], "wildcard column id mismatch");
        require(column.column_ordinal() == index, "wildcard column ordinal mismatch");
    }
}

void test_where_order_limit_and_offset()
{
    Fixture fixture;
    auto select = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT id, name FROM users WHERE age >= 18 "
        "ORDER BY age DESC LIMIT 10 OFFSET 20;"
    );
    require(select->where().has_value(), "WHERE missing");
    require(select->where()->type().id == LogicalTypeId::Boolean, "WHERE type mismatch");
    require(select->order_by().size() == 1, "ORDER BY count mismatch");
    require(!select->order_by()[0].ascending, "ORDER BY direction mismatch");
    require(select->limit() == 10, "LIMIT mismatch");
    require(select->offset() == 20, "OFFSET mismatch");
}

void test_order_by_alias_rules()
{
    Fixture fixture;
    auto alias = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT age + 1 AS next_age FROM users ORDER BY next_age DESC;"
    );
    require(alias->order_by().size() == 1, "alias ORDER BY missing");
    require(alias->order_by()[0].expression->kind() == BoundExpressionKind::Binary, "alias target mismatch");

    auto shadows_column = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT name AS age FROM users ORDER BY age;"
    );
    require(
        shadows_column->order_by()[0].expression->type().id == LogicalTypeId::Varchar,
        "ORDER BY should prefer projection alias"
    );

    auto duplicates = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT age AS x, name AS x FROM users;"
    );
    require(duplicates->projections()[0].output_name == "x", "first duplicate output name mismatch");
    require(duplicates->projections()[1].output_name == "x", "second duplicate output name mismatch");
    require_error(
        fixture,
        "SELECT age AS x, name AS x FROM users ORDER BY x;",
        BinderErrorCode::AmbiguousAlias
    );
}

void test_query_errors_and_database_context()
{
    Fixture fixture;
    auto missing_database = bind_error(
        fixture,
        "SELECT * FROM users;",
        std::nullopt
    );
    require(missing_database.is(BinderErrorCode::DatabaseNotSelected), "database context error mismatch");

    require_error(fixture, "SELECT missing FROM users;", BinderErrorCode::ColumnNotFound);
    require_error(fixture, "SELECT other.id FROM users;", BinderErrorCode::InvalidQualifier);
    require_error(
        fixture,
        "SELECT id FROM users WHERE age + 1;",
        BinderErrorCode::InvalidType
    );
}

void run_suite()
{
    test_projection_names_and_wildcard_order();
    test_where_order_limit_and_offset();
    test_order_by_alias_rules();
    test_query_errors_and_database_context();
}

} // namespace

int main()
{
    try {
        run_suite();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
