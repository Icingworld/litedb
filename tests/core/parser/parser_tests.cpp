#include "core/common/logical_type.hpp"
#include "core/parser/parser.hpp"
#include "core/parser/ast/expression/alias_expression.hpp"
#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_collection_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/show_collections_statement.hpp"
#include "core/parser/ast/statement/show_databases_statement.hpp"
#include "core/parser/ast/statement/show_indexes_statement.hpp"
#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{

using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;
using namespace litedb::core::common;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
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

ParserError parse_error(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    require(!result.has_value(), "statement should fail to parse");
    return std::move(result.error());
}

TokenLocation error_location(const ParserError & error)
{
    const auto * context = error.context<ParserErrorContext>();
    require(context != nullptr, "parser error should retain token location");
    return context->location;
}

void test_parse_use_statement()
{
    auto statement = parse_ok("USE demo;");

    require(statement->kind() == AstNodeKind::Use, "USE statement kind mismatch");
    const auto * use_statement = static_cast<const UseStatement *>(statement.get());
    require(use_statement->database() == "demo", "USE database name mismatch");
    require(use_statement->location().line == 1, "USE statement line mismatch");
    require(use_statement->location().column == 1, "USE statement column mismatch");

    auto without_semicolon = parse_ok("USE demo");
    require(without_semicolon->kind() == AstNodeKind::Use, "USE without semicolon should parse");
}

void test_parse_create_database_statement()
{
    auto statement = parse_ok("CREATE DATABASE IF NOT EXISTS demo;");

    require(statement->kind() == AstNodeKind::CreateDatabase, "CREATE DATABASE kind mismatch");
    const auto * create = static_cast<const CreateDatabaseStatement *>(statement.get());
    require(create->database() == "demo", "CREATE DATABASE name mismatch");
    require(create->if_not_exists(), "CREATE DATABASE IF NOT EXISTS mismatch");
}

void test_parse_create_collection_statement()
{
    auto statement = parse_ok(
        "CREATE COLLECTION users ("
        "id BIGINT NOT NULL, "
        "name VARCHAR(64) UNIQUE COMMENT 'display name', "
        "age INTEGER NULL DEFAULT 0, "
        "active BOOLEAN DEFAULT true, "
        "embedding VECTOR(128) DEFAULT [0.1, 0.2]"
        ") COMMENT 'user collection';"
    );

    require(statement->kind() == AstNodeKind::CreateCollection, "CREATE COLLECTION kind mismatch");
    const auto * create = static_cast<const CreateCollectionStatement *>(statement.get());
    require(create->collection() == "users", "CREATE COLLECTION name mismatch");
    require(create->columns().size() == 5, "CREATE COLLECTION column count mismatch");
    require(create->comment().has_value(), "CREATE COLLECTION comment missing");
    require(create->comment().value() == "user collection", "CREATE COLLECTION comment mismatch");
    require(!create->columns()[0].nullable, "NOT NULL constraint mismatch");
    require(create->columns()[1].unique, "UNIQUE constraint mismatch");
    require(create->columns()[1].comment.has_value(), "COMMENT constraint mismatch");
    require(create->columns()[1].type.id == LogicalTypeId::Varchar, "VARCHAR type mismatch");
    require(create->columns()[1].type.parameter.value() == 64, "VARCHAR length mismatch");
    require(create->columns()[1].location.line == 1, "column definition line mismatch");
    require(create->columns()[1].location.column > create->columns()[0].location.column, "column definition column mismatch");
    require(create->columns()[2].nullable, "NULL constraint mismatch");
    require(create->columns()[2].default_value != nullptr, "DEFAULT literal missing");
    require(create->columns()[4].type.id == LogicalTypeId::Vector, "VECTOR type mismatch");
    require(create->columns()[4].type.parameter.value() == 128, "VECTOR dimension mismatch");
    require(create->columns()[4].default_value->kind() == AstNodeKind::Vector, "VECTOR default mismatch");
}

void test_parse_create_index_statement()
{
    auto statement = parse_ok("CREATE INDEX IF NOT EXISTS idx_age ON users(age) USING BTREE;");

    require(statement->kind() == AstNodeKind::CreateIndex, "CREATE INDEX kind mismatch");
    const auto * create = static_cast<const CreateIndexStatement *>(statement.get());
    require(create->index_name() == "idx_age", "CREATE INDEX name mismatch");
    require(create->collection_name() == "users", "CREATE INDEX collection mismatch");
    require(create->column_name() == "age", "CREATE INDEX column mismatch");
    require(create->if_not_exists(), "CREATE INDEX IF NOT EXISTS mismatch");
    require(create->method() == CreateIndexMethod::BTree, "CREATE INDEX BTREE method mismatch");

    auto btree_statement = parse_ok("CREATE INDEX idx_name ON users(name) USING BTREE;");
    const auto * btree_create = static_cast<const CreateIndexStatement *>(btree_statement.get());
    require(btree_create->method() == CreateIndexMethod::BTree, "CREATE INDEX BTREE method mismatch");

    auto default_statement = parse_ok("CREATE INDEX idx_id ON users(id);");
    const auto * default_create = static_cast<const CreateIndexStatement *>(default_statement.get());
    require(default_create->method() == CreateIndexMethod::Default, "CREATE INDEX default method mismatch");
}

void test_parse_create_vector_index_statement()
{
    auto statement = parse_ok(
        "CREATE VINDEX IF NOT EXISTS vidx_embedding ON users(embedding) USING HNSW "
        "WITH (metric = COSINE, max_neighbors = 16, ef_construction = 200, ef_search = 64, random_seed = 7);"
    );

    require(statement->kind() == AstNodeKind::CreateVectorIndex, "CREATE VINDEX kind mismatch");
    const auto * create = static_cast<const CreateVectorIndexStatement *>(statement.get());
    require(create->index_name() == "vidx_embedding", "CREATE VINDEX name mismatch");
    require(create->collection_name() == "users", "CREATE VINDEX collection mismatch");
    require(create->column_name() == "embedding", "CREATE VINDEX column mismatch");
    require(create->if_not_exists(), "CREATE VINDEX IF NOT EXISTS mismatch");
    require(create->method() == CreateVectorIndexMethod::Hnsw, "CREATE VINDEX method mismatch");
    require(create->options().metric == VectorIndexMetric::Cosine, "CREATE VINDEX metric mismatch");
    require(create->options().max_neighbors.value() == 16, "CREATE VINDEX max_neighbors mismatch");
    require(create->options().ef_construction.value() == 200, "CREATE VINDEX ef_construction mismatch");
    require(create->options().ef_search.value() == 64, "CREATE VINDEX ef_search mismatch");
    require(create->options().random_seed.value() == 7, "CREATE VINDEX random_seed mismatch");

    auto minimal = parse_ok("CREATE VINDEX vidx_embedding ON users(embedding) USING hnsw;");
    const auto * minimal_create = static_cast<const CreateVectorIndexStatement *>(minimal.get());
    require(minimal_create->options().metric == VectorIndexMetric::Default, "CREATE VINDEX default metric mismatch");

    require(parse_error("CREATE VINDEX vidx_embedding ON users(embedding);").is(ParserErrorCode::ExpectedToken), "CREATE VINDEX missing USING error mismatch");

    auto bad_method = parse_error("CREATE VINDEX vidx_embedding ON users(embedding) USING IVF;");
    require(bad_method.is(ParserErrorCode::UnsupportedSyntax), "CREATE VINDEX method error mismatch");
    require(error_location(bad_method).column == 56, "CREATE VINDEX method error location mismatch");

    auto bad_metric = parse_error("CREATE VINDEX vidx_embedding ON users(embedding) USING HNSW WITH (metric = BAD);");
    require(bad_metric.is(ParserErrorCode::UnsupportedSyntax), "CREATE VINDEX metric error mismatch");
    require(error_location(bad_metric).column == 76, "CREATE VINDEX metric error location mismatch");

    auto duplicate_option = parse_error("CREATE VINDEX vidx_embedding ON users(embedding) USING HNSW WITH (metric = L2, metric = COSINE);");
    require(duplicate_option.is(ParserErrorCode::UnsupportedSyntax), "CREATE VINDEX duplicate option error mismatch");
    require(error_location(duplicate_option).column == 80, "CREATE VINDEX duplicate option error location mismatch");
}

void test_parse_drop_show_describe_statements()
{
    auto drop_database = parse_ok("DROP DATABASE IF EXISTS demo;");
    require(drop_database->kind() == AstNodeKind::DropDatabase, "DROP DATABASE kind mismatch");
    const auto * drop_db = static_cast<const DropDatabaseStatement *>(drop_database.get());
    require(drop_db->database_name() == "demo", "DROP DATABASE name mismatch");
    require(drop_db->if_exists(), "DROP DATABASE IF EXISTS mismatch");

    auto drop_collection = parse_ok("DROP COLLECTION users;");
    require(drop_collection->kind() == AstNodeKind::DropCollection, "DROP COLLECTION kind mismatch");
    const auto * drop_col = static_cast<const DropCollectionStatement *>(drop_collection.get());
    require(drop_col->collection_name() == "users", "DROP COLLECTION name mismatch");

    auto drop_index = parse_ok("DROP INDEX idx_age ON users;");
    require(drop_index->kind() == AstNodeKind::DropIndex, "DROP INDEX kind mismatch");
    const auto * drop_idx = static_cast<const DropIndexStatement *>(drop_index.get());
    require(drop_idx->index_name() == "idx_age", "DROP INDEX name mismatch");
    require(drop_idx->collection_name() == "users", "DROP INDEX collection mismatch");
    require(!drop_idx->if_exists(), "DROP INDEX IF EXISTS mismatch");

    auto drop_index_if_exists = parse_ok("DROP INDEX IF EXISTS idx_age ON users;");
    const auto * drop_idx_if_exists = static_cast<const DropIndexStatement *>(drop_index_if_exists.get());
    require(drop_idx_if_exists->collection_name() == "users", "DROP INDEX IF EXISTS collection mismatch");
    require(drop_idx_if_exists->if_exists(), "DROP INDEX IF EXISTS mismatch");

    auto drop_vector_index = parse_ok("DROP VINDEX IF EXISTS vidx_embedding ON users;");
    require(drop_vector_index->kind() == AstNodeKind::DropVectorIndex, "DROP VINDEX kind mismatch");
    const auto * drop_vidx = static_cast<const DropVectorIndexStatement *>(drop_vector_index.get());
    require(drop_vidx->index_name() == "vidx_embedding", "DROP VINDEX name mismatch");
    require(drop_vidx->collection_name() == "users", "DROP VINDEX collection mismatch");
    require(drop_vidx->if_exists(), "DROP VINDEX IF EXISTS mismatch");

    auto show_databases = parse_ok("SHOW DATABASES;");
    require(show_databases->kind() == AstNodeKind::ShowDatabases, "SHOW DATABASES kind mismatch");

    auto show_collections = parse_ok("SHOW COLLECTIONS;");
    require(show_collections->kind() == AstNodeKind::ShowCollections, "SHOW COLLECTIONS kind mismatch");
    const auto * show_col = static_cast<const ShowCollectionsStatement *>(show_collections.get());
    require(!show_col->database_name().has_value(), "SHOW COLLECTIONS database name mismatch");

    auto show_collections_from = parse_ok("SHOW COLLECTIONS FROM demo;");
    require(show_collections_from->kind() == AstNodeKind::ShowCollections, "SHOW COLLECTIONS FROM kind mismatch");
    const auto * show_col_from = static_cast<const ShowCollectionsStatement *>(show_collections_from.get());
    require(show_col_from->database_name().has_value(), "SHOW COLLECTIONS FROM database name missing");
    require(show_col_from->database_name().value() == "demo", "SHOW COLLECTIONS FROM database name mismatch");

    auto show_indexes = parse_ok("SHOW INDEXES FROM users;");
    require(show_indexes->kind() == AstNodeKind::ShowIndexes, "SHOW INDEXES kind mismatch");
    const auto * show_idx = static_cast<const ShowIndexesStatement *>(show_indexes.get());
    require(show_idx->collection_name() == "users", "SHOW INDEXES collection name mismatch");

    auto show_vector_indexes = parse_ok("SHOW VINDEXES FROM docs;");
    require(show_vector_indexes->kind() == AstNodeKind::ShowVectorIndexes, "SHOW VINDEXES kind mismatch");
    const auto * show_vidx = static_cast<const ShowVectorIndexesStatement *>(show_vector_indexes.get());
    require(show_vidx->collection_name() == "docs", "SHOW VINDEXES collection name mismatch");

    auto describe = parse_ok("DESCRIBE users;");
    const auto * describe_statement = static_cast<const DescribeCollectionStatement *>(describe.get());
    require(describe_statement->collection_name() == "users", "DESCRIBE collection name mismatch");

    auto desc = parse_ok("DESC users;");
    require(desc->kind() == AstNodeKind::DescribeCollection, "DESC statement kind mismatch");

    auto describe_collection = parse_ok("DESCRIBE COLLECTION users;");
    require(
        describe_collection->kind() == AstNodeKind::DescribeCollection,
        "DESCRIBE COLLECTION kind mismatch"
    );
    const auto * explicit_collection =
        static_cast<const DescribeCollectionStatement *>(describe_collection.get());
    require(explicit_collection->collection_name() == "users", "DESCRIBE COLLECTION name mismatch");
}

void test_parse_insert_statement()
{
    auto with_columns = parse_ok("INSERT INTO users (id, name, age, active) VALUES (1, 'Tom', 18, true);");
    require(with_columns->kind() == AstNodeKind::Insert, "INSERT kind mismatch");
    const auto * insert = static_cast<const InsertStatement *>(with_columns.get());
    require(insert->collection() == "users", "INSERT collection mismatch");
    require(insert->columns().size() == 4, "INSERT column count mismatch");
    require(insert->values().size() == 4, "INSERT value count mismatch");
    require(insert->values()[1]->kind() == AstNodeKind::Literal, "INSERT string value kind mismatch");

    auto without_columns = parse_ok("INSERT INTO users VALUES (2, 'Jerry', 20, true, [0.1, 0.2, 0.3]);");
    const auto * insert_without_columns = static_cast<const InsertStatement *>(without_columns.get());
    require(insert_without_columns->columns().empty(), "INSERT omitted columns mismatch");
    require(insert_without_columns->values()[4]->kind() == AstNodeKind::Vector, "INSERT vector value kind mismatch");
}

void test_parse_update_delete_statements()
{
    auto update = parse_ok("UPDATE users SET age = age + 1 WHERE name = 'Tom';");
    require(update->kind() == AstNodeKind::Update, "UPDATE kind mismatch");
    const auto * update_statement = static_cast<const UpdateStatement *>(update.get());
    require(update_statement->collection() == "users", "UPDATE collection mismatch");
    require(update_statement->assignments().size() == 1, "UPDATE assignment count mismatch");
    require(update_statement->assignments()[0].value->kind() == AstNodeKind::Binary, "UPDATE assignment expression mismatch");
    require(update_statement->where() != nullptr, "UPDATE WHERE missing");

    auto update_vector = parse_ok("UPDATE users SET active = false, embedding = [0.2, 0.3, 0.4] WHERE id = 1;");
    const auto * vector_update = static_cast<const UpdateStatement *>(update_vector.get());
    require(vector_update->assignments().size() == 2, "UPDATE multiple assignment count mismatch");
    require(vector_update->assignments()[1].value->kind() == AstNodeKind::Vector, "UPDATE vector assignment mismatch");

    auto delete_statement = parse_ok("DELETE FROM users WHERE age < 18;");
    require(delete_statement->kind() == AstNodeKind::Delete, "DELETE kind mismatch");
    const auto * del = static_cast<const DeleteStatement *>(delete_statement.get());
    require(del->collection() == "users", "DELETE collection mismatch");
    require(del->where() != nullptr, "DELETE WHERE missing");

    auto delete_all = parse_ok("DELETE FROM users;");
    const auto * del_all = static_cast<const DeleteStatement *>(delete_all.get());
    require(del_all->where() == nullptr, "DELETE without WHERE mismatch");
}

void test_parse_select_statement()
{
    auto statement = parse_ok("SELECT * FROM users WHERE age >= 18 ORDER BY age DESC LIMIT 10 OFFSET 20;");
    require(statement->kind() == AstNodeKind::Select, "SELECT kind mismatch");
    const auto * select = static_cast<const SelectStatement *>(statement.get());
    require(select->select_list().size() == 1, "SELECT list size mismatch");
    require(select->select_list()[0]->kind() == AstNodeKind::Wildcard, "SELECT wildcard mismatch");
    require(select->collection() == "users", "SELECT collection mismatch");
    require(select->where() != nullptr, "SELECT WHERE missing");
    require(select->order_by().size() == 1, "SELECT ORDER BY size mismatch");
    require(!select->order_by()[0].ascending, "SELECT ORDER BY direction mismatch");
    require(select->limit().value() == 10, "SELECT LIMIT mismatch");
    require(select->offset().value() == 20, "SELECT OFFSET mismatch");

    auto columns = parse_ok("SELECT id, users.name, *, users.* FROM users;");
    const auto * column_select = static_cast<const SelectStatement *>(columns.get());
    require(column_select->select_list().size() == 4, "SELECT qualified list size mismatch");
    require(column_select->select_list()[1]->kind() == AstNodeKind::ColumnReference, "SELECT qualified column mismatch");
    require(column_select->select_list()[2]->kind() == AstNodeKind::Wildcard, "SELECT wildcard mismatch");
    require(column_select->select_list()[3]->kind() == AstNodeKind::Wildcard, "SELECT qualified wildcard mismatch");

    auto function_order = parse_ok("SELECT id, l2_distance(embedding, [0.1, 0.2, 0.3]) FROM users ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC;");
    const auto * function_select = static_cast<const SelectStatement *>(function_order.get());
    require(function_select->select_list().size() == 2, "SELECT function list size mismatch");
    require(function_select->select_list()[1]->kind() == AstNodeKind::FunctionCall, "SELECT function item mismatch");
    require(function_select->order_by().size() == 1, "SELECT function ORDER BY size mismatch");
    require(function_select->order_by()[0].expression->kind() == AstNodeKind::FunctionCall, "SELECT function ORDER BY mismatch");

    auto expression_item = parse_ok("SELECT age + 1 FROM users;");
    const auto * expression_select = static_cast<const SelectStatement *>(expression_item.get());
    require(expression_select->select_list()[0]->kind() == AstNodeKind::Binary, "SELECT expression item mismatch");

    auto expression_alias = parse_ok("SELECT age + 1 AS next_age FROM users;");
    const auto * alias_select = static_cast<const SelectStatement *>(expression_alias.get());
    require(alias_select->select_list()[0]->kind() == AstNodeKind::Alias, "SELECT expression alias kind mismatch");
    const auto * next_age_alias = static_cast<const AliasExpression *>(alias_select->select_list()[0].get());
    require(next_age_alias->alias() == "next_age", "SELECT expression alias mismatch");
    require(next_age_alias->expression().kind() == AstNodeKind::Binary, "SELECT expression alias inner mismatch");

    auto literal_alias = parse_ok("SELECT 1 AS one FROM users;");
    const auto * literal_alias_select = static_cast<const SelectStatement *>(literal_alias.get());
    require(literal_alias_select->select_list()[0]->kind() == AstNodeKind::Alias, "SELECT literal alias kind mismatch");
    const auto * one_alias = static_cast<const AliasExpression *>(literal_alias_select->select_list()[0].get());
    require(one_alias->alias() == "one", "SELECT literal alias mismatch");
    require(one_alias->expression().kind() == AstNodeKind::Literal, "SELECT literal alias inner mismatch");

    auto parenthesized_alias = parse_ok("SELECT (age + score) AS total FROM users;");
    const auto * parenthesized_alias_select = static_cast<const SelectStatement *>(parenthesized_alias.get());
    const auto * total_alias = static_cast<const AliasExpression *>(parenthesized_alias_select->select_list()[0].get());
    require(total_alias->alias() == "total", "SELECT parenthesized alias mismatch");
    require(total_alias->expression().kind() == AstNodeKind::Binary, "SELECT parenthesized alias inner mismatch");

    auto function_expression_alias = parse_ok("SELECT l2_distance(embedding, [0.1, 0.2, 0.3]) + 1 AS score FROM users;");
    const auto * function_expression_alias_select = static_cast<const SelectStatement *>(function_expression_alias.get());
    const auto * score_alias = static_cast<const AliasExpression *>(function_expression_alias_select->select_list()[0].get());
    require(score_alias->alias() == "score", "SELECT function expression alias mismatch");
    require(score_alias->expression().kind() == AstNodeKind::Binary, "SELECT function expression alias inner mismatch");
}

void test_parse_expression_shapes()
{
    auto between_statement = parse_ok("SELECT * FROM users WHERE age BETWEEN 18 AND 30;");
    const auto * between_select = static_cast<const SelectStatement *>(between_statement.get());
    require(between_select->where()->kind() == AstNodeKind::Between, "BETWEEN expression kind mismatch");

    auto in_statement = parse_ok("SELECT * FROM users WHERE category IN ('book', 'tool');");
    const auto * in_select = static_cast<const SelectStatement *>(in_statement.get());
    require(in_select->where()->kind() == AstNodeKind::In, "IN expression kind mismatch");
    const auto * in_expression = static_cast<const InExpression *>(in_select->where());
    require(in_expression->values().size() == 2, "IN value count mismatch");

    auto like_statement = parse_ok("SELECT * FROM users WHERE name LIKE 'Tom%';");
    const auto * like_select = static_cast<const SelectStatement *>(like_statement.get());
    require(like_select->where()->kind() == AstNodeKind::Like, "LIKE expression kind mismatch");

    auto not_statement = parse_ok("SELECT * FROM users WHERE NOT active OR age < 10;");
    const auto * not_select = static_cast<const SelectStatement *>(not_statement.get());
    require(not_select->where()->kind() == AstNodeKind::Binary, "OR expression kind mismatch");
    const auto * or_expression = static_cast<const BinaryExpression *>(not_select->where());
    require(or_expression->op() == TokenType::Or, "OR operator mismatch");
    require(or_expression->left().kind() == AstNodeKind::Unary, "NOT expression kind mismatch");

    auto precedence_statement = parse_ok("SELECT * FROM users WHERE score + bonus * 2 >= 100;");
    const auto * precedence_select = static_cast<const SelectStatement *>(precedence_statement.get());
    require(precedence_select->where()->kind() == AstNodeKind::Binary, "comparison expression kind mismatch");
    const auto * comparison = static_cast<const BinaryExpression *>(precedence_select->where());
    require(comparison->op() == TokenType::GreaterEqual, "comparison operator mismatch");
    require(comparison->left().kind() == AstNodeKind::Binary, "additive expression kind mismatch");
}

void test_parse_failures()
{
    auto empty = parse_error("");
    require(empty.is(ParserErrorCode::EmptyStatement), "empty input error code mismatch");
    require(empty.message() == "Empty statement", "empty input error mismatch");

    auto trailing = parse_error("USE demo extra");
    require(trailing.is(ParserErrorCode::UnexpectedToken), "trailing token error code mismatch");
    require(error_location(trailing).column == 10, "trailing token location mismatch");

    auto multiple_semicolon = parse_error("USE demo;;");
    require(multiple_semicolon.is(ParserErrorCode::UnexpectedToken), "multiple semicolon error code mismatch");
    require(error_location(multiple_semicolon).column == 10, "multiple semicolon location mismatch");

    auto missing_name = parse_error("CREATE DATABASE;");
    require(missing_name.is(ParserErrorCode::ExpectedIdentifier), "missing object name error code mismatch");
    require(missing_name.message() == "Expected database name", "missing object name error mismatch");

    auto empty_columns = parse_error("CREATE COLLECTION users ();");
    require(empty_columns.is(ParserErrorCode::EmptyList), "empty column list error code mismatch");
    require(empty_columns.message() == "Expected at least one column definition", "empty column list error mismatch");

    auto missing_varchar_length = parse_error("CREATE COLLECTION users (name VARCHAR());");
    require(missing_varchar_length.is(ParserErrorCode::ExpectedToken), "VARCHAR missing length error code mismatch");
    require(missing_varchar_length.message() == "Expected VARCHAR length", "VARCHAR missing length error mismatch");

    auto missing_vector_dimension = parse_error("CREATE COLLECTION users (embedding VECTOR());");
    require(missing_vector_dimension.is(ParserErrorCode::ExpectedToken), "VECTOR missing dimension error code mismatch");
    require(missing_vector_dimension.message() == "Expected VECTOR dimension", "VECTOR missing dimension error mismatch");

    auto default_expression = parse_error("CREATE COLLECTION users (age INTEGER DEFAULT age);");
    require(default_expression.is(ParserErrorCode::ExpectedLiteral), "DEFAULT expression error code mismatch");
    require(default_expression.message() == "Expected literal after DEFAULT", "DEFAULT expression error mismatch");

    auto primary_constraint = parse_error("CREATE COLLECTION users (id BIGINT PRIMARY KEY);");
    require(primary_constraint.is(ParserErrorCode::UnexpectedToken), "PRIMARY KEY constraint error code mismatch");
    require(primary_constraint.message() == "Unexpected column constraint", "PRIMARY KEY constraint error mismatch");

    auto implicit_alias = parse_error("SELECT age + 1 next_age FROM users;");
    require(implicit_alias.is(ParserErrorCode::ExpectedToken), "implicit alias error code mismatch");
    require(implicit_alias.message() == "Expected FROM after select list", "implicit alias error mismatch");

    auto missing_alias = parse_error("SELECT age + 1 AS FROM users;");
    require(missing_alias.is(ParserErrorCode::ExpectedIdentifier), "missing alias error code mismatch");
    require(missing_alias.message() == "Expected alias after AS", "missing alias error mismatch");

    auto wildcard_alias = parse_error("SELECT * AS all_columns FROM users;");
    require(wildcard_alias.is(ParserErrorCode::UnexpectedToken), "wildcard alias error code mismatch");
    require(wildcard_alias.message() == "Wildcard select item cannot have alias", "wildcard alias error mismatch");

    auto qualified_wildcard_alias = parse_error("SELECT users.* AS all_user_columns FROM users;");
    require(qualified_wildcard_alias.is(ParserErrorCode::UnexpectedToken), "qualified wildcard alias error code mismatch");
    require(qualified_wildcard_alias.message() == "Wildcard select item cannot have alias", "qualified wildcard alias error mismatch");

    auto unsupported_group_by = parse_error("SELECT age FROM users GROUP BY age;");
    require(unsupported_group_by.is(ParserErrorCode::UnexpectedToken), "GROUP BY unsupported error code mismatch");
    require(unsupported_group_by.message() == "Unexpected token", "GROUP BY unsupported error mismatch");

    auto unsupported_index_method = parse_error("CREATE INDEX idx_age ON users(age) USING gin;");
    require(unsupported_index_method.is(ParserErrorCode::UnsupportedSyntax), "CREATE INDEX method error code mismatch");
    require(unsupported_index_method.message() == "Expected BTREE after USING", "CREATE INDEX method error mismatch");

    auto unsupported_b_tree = parse_error("CREATE INDEX idx_age ON users(age) USING B_TREE;");
    require(unsupported_b_tree.is(ParserErrorCode::UnsupportedSyntax), "CREATE INDEX B_TREE error code mismatch");
    require(unsupported_b_tree.message() == "Expected BTREE after USING", "CREATE INDEX B_TREE error mismatch");

    auto unsupported_method = parse_error("CREATE INDEX idx_age ON users(age) USING UNKNOWN;");
    require(unsupported_method.is(ParserErrorCode::UnsupportedSyntax), "unsupported index method error code mismatch");
    require(unsupported_method.message() == "Expected BTREE after USING", "unsupported index method error mismatch");

    auto show_indexes_missing_from = parse_error("SHOW INDEXES;");
    require(show_indexes_missing_from.is(ParserErrorCode::ExpectedToken), "SHOW INDEXES missing FROM error code mismatch");
    require(show_indexes_missing_from.message() == "Expected FROM after SHOW INDEXES", "SHOW INDEXES missing FROM error mismatch");

    auto show_vindexes_missing_from = parse_error("SHOW VINDEXES;");
    require(show_vindexes_missing_from.is(ParserErrorCode::ExpectedToken), "SHOW VINDEXES missing FROM error code mismatch");
    require(show_vindexes_missing_from.message() == "Expected FROM after SHOW VINDEXES", "SHOW VINDEXES missing FROM error mismatch");

    auto lexical_error = parse_error("SELECT ! FROM users;");
    require(lexical_error.is(ParserErrorCode::LexicalError), "lexical error code mismatch");
}

} // namespace

int main()
{
    try {
        test_parse_use_statement();
        test_parse_create_database_statement();
        test_parse_create_collection_statement();
        test_parse_create_index_statement();
        test_parse_create_vector_index_statement();
        test_parse_drop_show_describe_statements();
        test_parse_insert_statement();
        test_parse_update_delete_statements();
        test_parse_select_statement();
        test_parse_expression_shapes();
        test_parse_failures();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
