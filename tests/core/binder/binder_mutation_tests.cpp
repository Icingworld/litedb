#include "test_support.hpp"

#include <exception>
#include <iostream>

#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"

namespace
{

using namespace litedb::test::binder;

void test_insert_expands_catalog_row()
{
    Fixture fixture;
    auto insert = bind_ok<BoundInsertStatement>(
        fixture,
        "INSERT INTO users (embedding, id) VALUES ([0.1, 0.2, 0.3], 1);"
    );
    require(insert->collection_id() == fixture.users_id, "INSERT collection id mismatch");
    require(insert->values().size() == 4, "INSERT full-row expansion mismatch");
    require(insert->values()[0]->type().id == LogicalTypeId::BigInt, "id slot type mismatch");
    require(insert->values()[0]->kind() == BoundExpressionKind::Cast, "id widening cast missing");
    require(
        insert->values()[1]->type().id == LogicalTypeId::Varchar
            && insert->values()[1]->type().parameter == 64,
        "default slot type mismatch"
    );
    require(insert->values()[2]->kind() == BoundExpressionKind::Null, "nullable slot should be NULL");
    require(insert->values()[2]->type().id == LogicalTypeId::Integer, "typed NULL mismatch");
    require(
        insert->values()[3]->type().id == LogicalTypeId::Vector
            && insert->values()[3]->type().parameter == 3,
        "vector slot type mismatch"
    );

    auto full = bind_ok<BoundInsertStatement>(
        fixture,
        "INSERT INTO users VALUES (1, 'Tom', 18, [0.1, 0.2, 0.3]);"
    );
    require(full->values().size() == 4, "full INSERT value count mismatch");
}

void test_insert_errors()
{
    Fixture fixture;
    require_error(
        fixture,
        "INSERT INTO users (id, id) VALUES (1, 2);",
        BinderErrorCode::DuplicateColumn
    );
    require_error(
        fixture,
        "INSERT INTO users (id) VALUES (1, 2);",
        BinderErrorCode::InvalidValueCount
    );
    require_error(
        fixture,
        "INSERT INTO users (id, embedding) VALUES (1, [0.1, 0.2]);",
        BinderErrorCode::InvalidType
    );
    require_error(
        fixture,
        "INSERT INTO users (id) VALUES (NULL);",
        BinderErrorCode::NotNullable
    );
    require_error(
        fixture,
        "INSERT INTO users (id) VALUES (age);",
        BinderErrorCode::UnsupportedExpression
    );
    require_error(
        fixture,
        "INSERT INTO users (missing) VALUES (1);",
        BinderErrorCode::ColumnNotFound
    );
}

void test_update_binding()
{
    Fixture fixture;
    auto update = bind_ok<BoundUpdateStatement>(
        fixture,
        "UPDATE users SET age = age + 1, id = 2 WHERE id = 1;"
    );
    require(update->collection_id() == fixture.users_id, "UPDATE collection id mismatch");
    require(update->assignments().size() == 2, "UPDATE assignment count mismatch");
    require(update->assignments()[0].column_id == fixture.age_column_id, "age assignment id mismatch");
    require(update->assignments()[0].value->kind() == BoundExpressionKind::Binary, "row expression missing");
    require(update->assignments()[1].column_id == fixture.id_column_id, "id assignment id mismatch");
    require(update->assignments()[1].value->kind() == BoundExpressionKind::Cast, "assignment cast missing");
    require(update->where() != nullptr, "UPDATE WHERE missing");
    require(update->where()->type().id == LogicalTypeId::Boolean, "UPDATE WHERE type mismatch");
}

void test_update_errors()
{
    Fixture fixture;
    require_error(
        fixture,
        "UPDATE users SET age = 1, AGE = 2;",
        BinderErrorCode::DuplicateColumn
    );
    require_error(fixture, "UPDATE users SET missing = 1;", BinderErrorCode::ColumnNotFound);
    require_error(fixture, "UPDATE users SET age = name;", BinderErrorCode::InvalidType);
    require_error(fixture, "UPDATE users SET id = NULL;", BinderErrorCode::NotNullable);
    require_error(
        fixture,
        "UPDATE users SET age = 1 WHERE age + 1;",
        BinderErrorCode::InvalidType
    );
}

void test_delete_binding_and_errors()
{
    Fixture fixture;
    auto unconditional = bind_ok<BoundDeleteStatement>(fixture, "DELETE FROM users;");
    require(unconditional->collection_id() == fixture.users_id, "DELETE collection id mismatch");
    require(unconditional->where() == nullptr, "DELETE without WHERE mismatch");

    auto conditional = bind_ok<BoundDeleteStatement>(
        fixture,
        "DELETE FROM users WHERE id = 1;"
    );
    require(conditional->where() != nullptr, "DELETE WHERE missing");
    require(conditional->where()->type().id == LogicalTypeId::Boolean, "DELETE WHERE type mismatch");

    require_error(
        fixture,
        "DELETE FROM users WHERE age + 1;",
        BinderErrorCode::InvalidType
    );
    require_error(
        fixture,
        "DELETE FROM users WHERE missing = 1;",
        BinderErrorCode::ColumnNotFound
    );
}

void run_suite()
{
    test_insert_expands_catalog_row();
    test_insert_errors();
    test_update_binding();
    test_update_errors();
    test_delete_binding_and_errors();
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
