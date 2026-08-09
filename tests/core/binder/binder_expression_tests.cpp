#include "test_support.hpp"

#include <exception>
#include <iostream>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"

namespace
{

using namespace litedb::test::binder;

const BoundExpression & projection(const BoundSelectStatement & statement, std::size_t index)
{
    return *statement.projections()[index].expression;
}

void test_column_reference_identity()
{
    Fixture fixture;
    auto select = bind_ok<BoundSelectStatement>(fixture, "SELECT id, age FROM users;");

    const auto & id = static_cast<const BoundColumnRefExpression &>(projection(*select, 0));
    require(id.column_id() == fixture.id_column_id, "column id mismatch");
    require(id.column_ordinal() == 0, "id ordinal mismatch");
    require(id.type().id == LogicalTypeId::BigInt, "id type mismatch");

    const auto & age = static_cast<const BoundColumnRefExpression &>(projection(*select, 1));
    require(age.column_id() == fixture.age_column_id, "age column id mismatch");
    require(age.column_ordinal() == 2, "age ordinal mismatch");
}

void test_literals_are_typed_during_binding()
{
    Fixture fixture;
    auto select = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT 1, 1.5, 'name', TRUE FROM users;"
    );

    const auto & integer = static_cast<const BoundLiteralExpression &>(projection(*select, 0));
    require(std::get<std::int32_t>(integer.value().data()) == 1, "INTEGER literal value mismatch");
    const auto & number = static_cast<const BoundLiteralExpression &>(projection(*select, 1));
    require(std::get<double>(number.value().data()) == 1.5, "DOUBLE literal value mismatch");
    const auto & string = static_cast<const BoundLiteralExpression &>(projection(*select, 2));
    require(std::get<std::string>(string.value().data()) == "name", "VARCHAR literal value mismatch");
    const auto & boolean = static_cast<const BoundLiteralExpression &>(projection(*select, 3));
    require(std::get<bool>(boolean.value().data()), "BOOLEAN literal value mismatch");
}

void test_unary_binary_and_numeric_coercion()
{
    Fixture fixture;
    auto select = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT -age, NOT (age = 1), age + 1.5 FROM users;"
    );

    const auto & negate = static_cast<const BoundUnaryExpression &>(projection(*select, 0));
    require(negate.op() == UnaryOperator::Negate, "negate operator mismatch");

    const auto & logical_not = static_cast<const BoundUnaryExpression &>(projection(*select, 1));
    require(logical_not.op() == UnaryOperator::Not, "not operator mismatch");
    require(logical_not.type().id == LogicalTypeId::Boolean, "not type mismatch");

    const auto & add = static_cast<const BoundBinaryExpression &>(projection(*select, 2));
    require(add.op() == BinaryOperator::Add, "add operator mismatch");
    require(add.type().id == LogicalTypeId::Double, "numeric common type mismatch");
    require(add.left().kind() == BoundExpressionKind::Cast, "numeric left cast missing");
}

void test_predicates_and_string_compatibility()
{
    Fixture fixture;
    auto select = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT age IN (1, 2.5), age BETWEEN 1 AND 2.5, name LIKE 'A%' "
        "FROM users WHERE name = 'Alice';"
    );

    const auto & in = static_cast<const BoundInExpression &>(projection(*select, 0));
    require(in.expression().type().id == LogicalTypeId::Double, "IN target coercion mismatch");
    require(in.values().size() == 2, "IN value count mismatch");
    require(in.values()[0]->type().id == LogicalTypeId::Double, "IN value coercion mismatch");

    const auto & between = static_cast<const BoundBetweenExpression &>(projection(*select, 1));
    require(between.expression().type().id == LogicalTypeId::Double, "BETWEEN target coercion mismatch");
    require(between.lower().type().id == LogicalTypeId::Double, "BETWEEN lower coercion mismatch");
    require(between.upper().type().id == LogicalTypeId::Double, "BETWEEN upper coercion mismatch");

    require(projection(*select, 2).kind() == BoundExpressionKind::Like, "LIKE kind mismatch");
    require(select->where().has_value(), "VARCHAR equality WHERE missing");
    require(select->where()->type().id == LogicalTypeId::Boolean, "VARCHAR equality type mismatch");
}

void test_function_binding_and_vector_dimension()
{
    Fixture fixture;
    auto select = bind_ok<BoundSelectStatement>(
        fixture,
        "SELECT l2_distance(embedding, [0.1, 0.2, 0.3]) FROM users;"
    );
    const auto & function = static_cast<const BoundFunctionExpression &>(projection(*select, 0));
    require(function.function().name() == "l2_distance", "function identity mismatch");
    require(function.type().id == LogicalTypeId::Double, "function return type mismatch");
    require(function.arguments().size() == 2, "function argument count mismatch");

    auto vector_error = require_error(
        fixture,
        "SELECT l2_distance(embedding, [0.1, 0.2]) FROM users;",
        BinderErrorCode::InvalidType
    );
    require(vector_error.context<BinderErrorContext>() != nullptr, "function constraint location missing");
    require(vector_error.cause() != nullptr, "function constraint cause missing");
    require(
        vector_error.cause()->is(litedb::core::function::FunctionErrorCode::ConstraintViolation),
        "function constraint cause code mismatch"
    );
    auto missing_error = require_error(
        fixture,
        "SELECT missing_function(age) FROM users;",
        BinderErrorCode::FunctionNotFound
    );
    require(missing_error.context<BinderErrorContext>() != nullptr, "unknown function location missing");
    require(missing_error.cause() != nullptr, "unknown function cause missing");
    require(
        missing_error.cause()->is(litedb::core::function::FunctionErrorCode::FunctionNotFound),
        "unknown function cause code mismatch"
    );
}

void test_invalid_expression_context()
{
    Fixture fixture;
    auto error = require_error(
        fixture,
        "SELECT name + 1 FROM users;",
        BinderErrorCode::InvalidType
    );
    const auto * context = error.context<BinderErrorContext>();
    require(context != nullptr, "expression error location missing");
    require(context->location.line == 1, "expression error line mismatch");
    require(context->location.column > 0, "expression error column missing");
}

void run_suite()
{
    test_column_reference_identity();
    test_literals_are_typed_during_binding();
    test_unary_binary_and_numeric_coercion();
    test_predicates_and_string_compatibility();
    test_function_binding_and_vector_dimension();
    test_invalid_expression_context();
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
