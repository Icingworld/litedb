#include "core/parser/ast/debug_printer.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

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

std::unique_ptr<StatementNode> parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

std::string print_without_location(const AstNode & node)
{
    AstDebugPrinterOptions options;
    options.include_location = false;
    return debug_print(node, options);
}

void test_select_debug_print()
{
    auto statement = parse_ok("SELECT id FROM users WHERE age >= 18;");
    const auto output = debug_print(*statement);

    require_contains(output, "SelectStatement @1:1\n");
    require_contains(output, "  collection: users\n");
    require_contains(output, "  select_list:\n");
    require_contains(output, "    [0] ColumnReferenceExpression @1:8\n");
    require_contains(output, "      qualifier: <none>\n");
    require_contains(output, "      column: id\n");
    require_contains(output, "  where:\n");
    require_contains(output, "    BinaryExpression @");
    require_contains(output, "      op: GreaterEqual\n");
    require_contains(output, "  order_by: []\n");
    require_contains(output, "  limit: <none>\n");
    require_contains(output, "  offset: <none>\n");

    auto alias_statement = parse_ok("SELECT age + 1 AS next_age, *, users.* FROM users;");
    const auto alias_output = print_without_location(*alias_statement);
    require_contains(alias_output, "    [0] AliasExpression\n");
    require_contains(alias_output, "      expression:\n");
    require_contains(alias_output, "        BinaryExpression\n");
    require_contains(alias_output, "      alias: next_age\n");
    require_contains(alias_output, "    [1] WildcardExpression\n");
    require_contains(alias_output, "      qualifier: <none>\n");
    require_contains(alias_output, "    [2] WildcardExpression\n");
    require_contains(alias_output, "      qualifier: users\n");
}

void test_expression_debug_print()
{
    auto statement = parse_ok(
        "SELECT l2_distance(embedding, [0.1, 0.2]) FROM users "
        "WHERE age BETWEEN 18 AND 30 OR name IN ('Tom', 'Jerry') OR name LIKE 'A%';"
    );
    const auto output = print_without_location(*statement);

    require(output.find('@') == std::string::npos, "location should be omitted");
    require_contains(output, "FunctionCallExpression\n");
    require_contains(output, "  name: l2_distance\n");
    require_contains(output, "  arguments:\n");
    require_contains(output, "VectorExpression\n");
    require_contains(output, "BetweenExpression\n");
    require_contains(output, "InExpression\n");
    require_contains(output, "LikeExpression\n");
}

void test_create_collection_debug_print()
{
    auto statement = parse_ok(
        "CREATE COLLECTION users ("
        "id BIGINT NOT NULL, "
        "name VARCHAR(64) UNIQUE COMMENT 'display name', "
        "age INTEGER DEFAULT 0"
        ") COMMENT 'user collection';"
    );
    const auto output = print_without_location(*statement);

    require_contains(output, "CreateCollectionStatement\n");
    require_contains(output, "  collection: users\n");
    require_contains(output, "  comment: user collection\n");
    require_contains(output, "  columns:\n");
    require_contains(output, "    [0] ColumnDefinition\n");
    require_contains(output, "      name: id\n");
    require_contains(output, "      nullable: false\n");
    require_contains(output, "    [1] ColumnDefinition\n");
    require_contains(output, "      kind: Varchar\n");
    require_contains(output, "      parameter: 64\n");
    require_contains(output, "      unique: true\n");
    require_contains(output, "      comment: display name\n");
    require_contains(output, "      default_value:\n");
    require_contains(output, "        LiteralExpression\n");
}

void test_insert_update_delete_debug_print()
{
    auto insert = parse_ok("INSERT INTO users VALUES (1, 'Tom');");
    const auto insert_output = print_without_location(*insert);
    require_contains(insert_output, "InsertStatement\n");
    require_contains(insert_output, "  columns: []\n");
    require_contains(insert_output, "  values:\n");
    require_contains(insert_output, "    [0] LiteralExpression\n");

    auto update = parse_ok("UPDATE users SET age = age + 1;");
    const auto update_output = print_without_location(*update);
    require_contains(update_output, "UpdateStatement\n");
    require_contains(update_output, "  assignments:\n");
    require_contains(update_output, "    [0] Assignment\n");
    require_contains(update_output, "      column: age\n");
    require_contains(update_output, "  where: <none>\n");

    auto del = parse_ok("DELETE FROM users;");
    const auto delete_output = print_without_location(*del);
    require_contains(delete_output, "DeleteStatement\n");
    require_contains(delete_output, "  where: <none>\n");
}

void test_vector_index_debug_print()
{
    auto statement = parse_ok(
        "CREATE VINDEX IF NOT EXISTS vidx_embedding ON users(embedding) USING HNSW "
        "WITH (metric = COSINE, max_neighbors = 16, ef_search = 64);"
    );
    const auto output = print_without_location(*statement);

    require_contains(output, "CreateVectorIndexStatement\n");
    require_contains(output, "  index_name: vidx_embedding\n");
    require_contains(output, "  if_not_exists: true\n");
    require_contains(output, "  method: Hnsw\n");
    require_contains(output, "  options:\n");
    require_contains(output, "    metric: Cosine\n");
    require_contains(output, "    max_neighbors: 16\n");
    require_contains(output, "    ef_construction: <none>\n");
    require_contains(output, "    ef_search: 64\n");
    require_contains(output, "    random_seed: <none>\n");
}

} // namespace

int main()
{
    try {
        test_select_debug_print();
        test_expression_debug_print();
        test_create_collection_debug_print();
        test_insert_update_delete_debug_print();
        test_vector_index_debug_print();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
