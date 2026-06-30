#include "core/binder/binder.hpp"
#include "core/binder/bound/debug_printer.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/session_context.hpp"
#include "core/catalog/in_memory_catalog.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using namespace litedb::core::binder;
using namespace litedb::core::binder::bound;
using namespace litedb::core::catalog;
using namespace litedb::core::common;
using namespace litedb::core::parser;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_contains(const std::string & text, std::string_view expected)
{
    if (text.find(expected) == std::string::npos) {
        throw std::runtime_error("debug output missing expected text: " + std::string(expected));
    }
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

std::unique_ptr<litedb::core::parser::ast::StatementNode> parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(std::string(result.error().message).append(": ").append(sql));
    }
    return std::move(result.value());
}

struct Fixture
{
    InMemoryCatalog catalog;
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        if (!database.has_value()) {
            throw std::runtime_error(database.error().message);
        }
        database_id = database.value();

        CreateCollectionRequest users;
        users.database_id = database_id;
        users.name = "users";
        users.columns = {
            ColumnDefinition {
                .name = "id",
                .type = type(LogicalTypeId::BigInt),
                .primary_key = true,
            },
            ColumnDefinition {
                .name = "name",
                .type = type(LogicalTypeId::Varchar, 64),
                .default_expression = CatalogDefaultExpression::literal(CatalogDefaultLiteralKind::String, "unknown"),
            },
            ColumnDefinition {
                .name = "age",
                .type = type(LogicalTypeId::Integer),
                .nullable = true,
            },
            ColumnDefinition {
                .name = "embedding",
                .type = type(LogicalTypeId::Vector, 3),
                .nullable = true,
            },
        };

        auto collection = catalog.create_collection(users);
        if (!collection.has_value()) {
            throw std::runtime_error(collection.error().message);
        }
        users_id = collection.value();
    }
};

std::unique_ptr<BoundStatement> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    BinderContext context {fixture.catalog, session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::string print_without_location(const BoundStatement & statement)
{
    BoundDebugPrinterOptions options;
    options.include_location = false;
    return debug_print(statement, options);
}

void test_select_debug_print()
{
    Fixture fixture;
    auto statement = bind_ok(fixture, "SELECT id FROM users WHERE age >= 18 ORDER BY age DESC LIMIT 10;");
    const auto output = debug_print(*statement);

    require_contains(output, "BoundSelectStatement @1:1\n");
    require_contains(output, "  database_id: 1\n");
    require_contains(output, "  collection_id: 1\n");
    require_contains(output, "  collection_name: users\n");
    require_contains(output, "  projections:\n");
    require_contains(output, "    [0] BoundColumnRefExpression @1:8\n");
    require_contains(output, "      type: BIGINT\n");
    require_contains(output, "      column_name: id\n");
    require_contains(output, "  where:\n");
    require_contains(output, "    BoundBinaryExpression @");
    require_contains(output, "      type: BOOLEAN\n");
    require_contains(output, "      op: GreaterEqual\n");
    require_contains(output, "  order_by:\n");
    require_contains(output, "    [0] BoundOrderByItem\n");
    require_contains(output, "      ascending: false\n");
    require_contains(output, "  limit: 10\n");
    require_contains(output, "  offset: <none>\n");
}

void test_insert_update_debug_print()
{
    Fixture fixture;
    auto insert = bind_ok(fixture, "INSERT INTO users (id, age, embedding) VALUES (1, 18, [0.1, 0.2, 0.3]);");
    const auto insert_output = print_without_location(*insert);

    require(insert_output.find('@') == std::string::npos, "location should be omitted");
    require_contains(insert_output, "BoundInsertStatement\n");
    require_contains(insert_output, "  columns:\n");
    require_contains(insert_output, "    [0] BoundColumn\n");
    require_contains(insert_output, "      name: id\n");
    require_contains(insert_output, "      type: BIGINT\n");
    require_contains(insert_output, "  values:\n");
    require_contains(insert_output, "    [1] BoundCastExpression\n");
    require_contains(insert_output, "      target_type: VARCHAR(64)\n");
    require_contains(insert_output, "BoundVectorExpression\n");
    require_contains(insert_output, "      type: VECTOR(3)\n");

    auto update = bind_ok(fixture, "UPDATE users SET age = age + 1 WHERE id = 1;");
    const auto update_output = print_without_location(*update);
    require_contains(update_output, "BoundUpdateStatement\n");
    require_contains(update_output, "  assignments:\n");
    require_contains(update_output, "    [0] BoundAssignment\n");
    require_contains(update_output, "      column:\n");
    require_contains(update_output, "        BoundColumn\n");
    require_contains(update_output, "          name: age\n");
    require_contains(update_output, "      value:\n");
    require_contains(update_output, "        BoundBinaryExpression\n");
    require_contains(update_output, "  where:\n");
}

void test_index_and_show_debug_print()
{
    Fixture fixture;
    auto create_index = bind_ok(fixture, "CREATE INDEX idx_age ON users (age);");
    const auto create_index_output = print_without_location(*create_index);
    require_contains(create_index_output, "BoundCreateIndexStatement\n");
    require_contains(create_index_output, "  collection_name: users\n");
    require_contains(create_index_output, "  column_name: age\n");
    require_contains(create_index_output, "  index_name: idx_age\n");
    require_contains(create_index_output, "  index_kind: BTree\n");

    auto create_vindex = bind_ok(
        fixture,
        "CREATE VINDEX vidx_embedding ON users (embedding) USING HNSW "
        "WITH (metric = COSINE, max_neighbors = 24, ef_construction = 240, ef_search = 80, random_seed = 9);"
    );
    const auto create_vindex_output = print_without_location(*create_vindex);
    require_contains(create_vindex_output, "BoundCreateVectorIndexStatement\n");
    require_contains(create_vindex_output, "  column_name: embedding\n");
    require_contains(create_vindex_output, "  index_name: vidx_embedding\n");
    require_contains(create_vindex_output, "  index_kind: Hnsw\n");
    require_contains(create_vindex_output, "  metric: Cosine\n");
    require_contains(create_vindex_output, "  max_neighbors: 24\n");
    require_contains(create_vindex_output, "  ef_construction: 240\n");
    require_contains(create_vindex_output, "  ef_search_default: 80\n");
    require_contains(create_vindex_output, "  random_seed: 9\n");

    auto show_indexes = bind_ok(fixture, "SHOW INDEXES FROM users;");
    const auto show_indexes_output = print_without_location(*show_indexes);
    require_contains(show_indexes_output, "BoundShowIndexesStatement\n");
    require_contains(show_indexes_output, "  database_id: 1\n");
    require_contains(show_indexes_output, "  collection_id: 1\n");
    require_contains(show_indexes_output, "  collection_name: users\n");

    auto show_vindexes = bind_ok(fixture, "SHOW VINDEXES FROM users;");
    const auto show_vindexes_output = print_without_location(*show_vindexes);
    require_contains(show_vindexes_output, "BoundShowVectorIndexesStatement\n");
    require_contains(show_vindexes_output, "  collection_name: users\n");
}

} // namespace

int main()
{
    try {
        test_select_debug_print();
        test_insert_update_debug_print();
        test_index_and_show_debug_print();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
