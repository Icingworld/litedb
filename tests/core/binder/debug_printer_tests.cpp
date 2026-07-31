#include "core/binder/bound/debug/debug_printer.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_contains(const std::string & text, std::string_view expected)
{
    if (text.find(expected) == std::string::npos) {
        throw std::runtime_error(
            "debug output missing expected text: " + std::string(expected)
        );
    }
}

common::LogicalType type(
    common::LogicalTypeId id,
    std::optional<std::size_t> parameter = std::nullopt
)
{
    return common::LogicalType {id, parameter};
}

std::unique_ptr<BoundExpression> literal(
    common::LogicalTypeId id,
    std::string value
)
{
    return std::make_unique<BoundLiteralExpression>(type(id), std::move(value));
}

void test_expression_debug_print()
{
    BoundBinaryExpression binary(
        literal(common::LogicalTypeId::Integer, "1"),
        common::BinaryOperator::Add,
        literal(common::LogicalTypeId::Integer, "2"),
        type(common::LogicalTypeId::Integer)
    );
    auto output = debug_print(binary);
    require_contains(output, "BoundBinaryExpression\n");
    require_contains(output, "  type: INTEGER\n");
    require_contains(output, "  op: Add\n");
    require_contains(output, "  left:\n    BoundLiteralExpression\n");
    require_contains(output, "  right:\n    BoundLiteralExpression\n");

    BoundDebugPrinterOptions options {.include_type = false};
    const auto without_type = debug_print(binary, options);
    require(
        without_type.find("type:") == std::string::npos,
        "type fields should be omitted"
    );

    BoundUnaryExpression unary(
        common::UnaryOperator::Negate,
        literal(common::LogicalTypeId::Integer, "1"),
        type(common::LogicalTypeId::Integer)
    );
    require_contains(debug_print(unary), "  op: Negate\n");

    BoundColumnRefExpression column(
        7,
        2,
        type(common::LogicalTypeId::BigInt)
    );
    output = debug_print(column);
    require_contains(output, "  column_id: 7\n");
    require_contains(output, "  column_ordinal: 2\n");

    BoundNullExpression null_expression(type(common::LogicalTypeId::Null));
    require_contains(debug_print(null_expression), "  type: NULL\n");

    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.push_back(literal(common::LogicalTypeId::Double, "0.1"));
    elements.push_back(literal(common::LogicalTypeId::Double, "0.2"));
    BoundVectorExpression vector(std::move(elements));
    output = debug_print(vector);
    require_contains(output, "BoundVectorExpression\n");
    require_contains(output, "    [0] BoundLiteralExpression\n");
    require_contains(output, "    [1] BoundLiteralExpression\n");

    auto function = std::make_shared<function::ScalarFunction>(
        "identity",
        std::vector<function::FunctionSignature> {},
        nullptr
    );
    std::vector<std::unique_ptr<BoundExpression>> arguments;
    arguments.push_back(literal(common::LogicalTypeId::Integer, "1"));
    BoundFunctionExpression function_expression(
        std::move(function),
        std::move(arguments),
        type(common::LogicalTypeId::Integer)
    );
    output = debug_print(function_expression);
    require_contains(output, "BoundFunctionExpression\n");
    require_contains(output, "  name: identity\n");
    require_contains(output, "    [0] BoundLiteralExpression\n");

    std::vector<std::unique_ptr<BoundExpression>> values;
    values.push_back(literal(common::LogicalTypeId::Integer, "1"));
    BoundInExpression in_expression(
        literal(common::LogicalTypeId::Integer, "1"),
        std::move(values)
    );
    require_contains(debug_print(in_expression), "BoundInExpression\n");

    BoundBetweenExpression between(
        literal(common::LogicalTypeId::Integer, "2"),
        literal(common::LogicalTypeId::Integer, "1"),
        literal(common::LogicalTypeId::Integer, "3")
    );
    output = debug_print(between);
    require_contains(output, "  lower:\n");
    require_contains(output, "  upper:\n");

    BoundLikeExpression like(
        literal(common::LogicalTypeId::Varchar, "name"),
        literal(common::LogicalTypeId::Varchar, "n%")
    );
    require_contains(debug_print(like), "  pattern:\n");

    BoundCastExpression cast(
        literal(common::LogicalTypeId::Integer, "1"),
        type(common::LogicalTypeId::BigInt)
    );
    output = debug_print(cast);
    require_contains(output, "BoundCastExpression\n");
    require_contains(output, "  target_type: BIGINT\n");
}

void test_statement_debug_print()
{
    BoundCreateDatabaseStatement create_database(std::string("demo"));
    require_contains(
        debug_print(create_database),
        "  database_name: demo\n"
    );

    std::vector<meta::ColumnDefinition> columns {
        meta::ColumnDefinition {
            .name = "id",
            .type = type(common::LogicalTypeId::BigInt),
            .unique = true,
            .nullable = false,
            .default_expression = std::nullopt,
            .comment = std::nullopt,
        },
    };
    BoundCreateCollectionStatement create_collection(
        1,
        std::string("users"),
        std::move(columns),
        std::nullopt
    );
    auto output = debug_print(create_collection);
    require_contains(output, "BoundCreateCollectionStatement\n");
    require_contains(output, "  database_id: 1\n");
    require_contains(output, "    [0] ColumnDefinition\n");
    require_contains(output, "      type: BIGINT\n");

    BoundCreateIndexStatement create_index(
        7,
        std::string("idx_id"),
        meta::entry::IndexKind::BTree,
        true
    );
    output = debug_print(create_index);
    require_contains(output, "  index_kind: BTree\n");
    require_contains(output, "  unique: true\n");

    BoundCreateVectorIndexStatement create_vector_index(
        8,
        std::string("vidx_embedding"),
        meta::entry::VectorIndexKind::Hnsw,
        meta::entry::VectorDistanceMetric::Cosine,
        24,
        240,
        80,
        9
    );
    output = debug_print(create_vector_index);
    require_contains(output, "  vector_index_kind: Hnsw\n");
    require_contains(output, "  metric: Cosine\n");
    require_contains(output, "  random_seed: 9\n");

    BoundDeleteStatement delete_statement(3, nullptr);
    output = debug_print(delete_statement);
    require_contains(output, "BoundDeleteStatement\n");
    require_contains(output, "  where: <none>\n");

    BoundDescribeCollectionStatement describe(3);
    require_contains(debug_print(describe), "  collection_id: 3\n");

    BoundDropDatabaseStatement drop_database(std::nullopt);
    require_contains(debug_print(drop_database), "  database_id: <none>\n");
    BoundDropCollectionStatement drop_collection(3);
    require_contains(debug_print(drop_collection), "  collection_id: 3\n");
    BoundDropIndexStatement drop_index(4);
    require_contains(debug_print(drop_index), "  index_id: 4\n");
    BoundDropVectorIndexStatement drop_vector_index(5);
    require_contains(debug_print(drop_vector_index), "  vector_index_id: 5\n");

    std::vector<std::unique_ptr<BoundExpression>> insert_values;
    insert_values.push_back(literal(common::LogicalTypeId::Integer, "1"));
    BoundInsertStatement insert(3, std::move(insert_values));
    output = debug_print(insert);
    require_contains(output, "BoundInsertStatement\n");
    require_contains(output, "    [0] BoundLiteralExpression\n");

    std::vector<BoundProjectionItem> projections;
    projections.push_back(BoundProjectionItem {
        .expression = std::make_unique<BoundColumnRefExpression>(
            7,
            0,
            type(common::LogicalTypeId::BigInt)
        ),
        .output_name = "id",
    });
    std::vector<BoundOrderByItem> order_by;
    order_by.push_back(BoundOrderByItem {
        .expression = std::make_unique<BoundColumnRefExpression>(
            7,
            0,
            type(common::LogicalTypeId::BigInt)
        ),
        .ascending = false,
    });
    BoundSelectStatement select(
        3,
        std::move(projections),
        nullptr,
        std::move(order_by),
        10,
        std::nullopt
    );
    output = debug_print(select);
    require_contains(output, "BoundSelectStatement\n");
    require_contains(output, "      output_name: id\n");
    require_contains(output, "      ascending: false\n");
    require_contains(output, "  limit: 10\n");

    BoundShowDatabasesStatement show_databases;
    require_contains(debug_print(show_databases), "BoundShowDatabasesStatement\n");
    BoundShowCollectionsStatement show_collections(1);
    require_contains(debug_print(show_collections), "  database_id: 1\n");
    BoundShowIndexesStatement show_indexes(3);
    require_contains(debug_print(show_indexes), "  collection_id: 3\n");
    BoundShowVectorIndexesStatement show_vector_indexes(3);
    require_contains(debug_print(show_vector_indexes), "  collection_id: 3\n");

    std::vector<BoundAssignment> assignments;
    assignments.push_back(BoundAssignment {
        .column_id = 7,
        .value = literal(common::LogicalTypeId::Integer, "2"),
    });
    BoundUpdateStatement update(3, std::move(assignments), nullptr);
    output = debug_print(update);
    require_contains(output, "BoundUpdateStatement\n");
    require_contains(output, "      column_id: 7\n");

    BoundUseStatement use(1);
    require_contains(debug_print(use), "  database_id: 1\n");
}

} // namespace

int main()
{
    try {
        test_expression_debug_print();
        test_statement_debug_print();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
