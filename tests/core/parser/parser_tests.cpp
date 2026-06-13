#include "core/parser/parser.hpp"
#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_statement.hpp"
#include "core/parser/ast/statement/drop_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/show_statement.hpp"
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
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

ParserError parse_error(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    require(!result.has_value(), "statement should fail to parse");
    return result.error();
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
        "id BIGINT PRIMARY KEY, "
        "name VARCHAR(64) UNIQUE COMMENT 'display name', "
        "age INTEGER DEFAULT 0, "
        "active BOOLEAN DEFAULT true, "
        "embedding VECTOR(128) DEFAULT [0.1, 0.2]"
        ");"
    );

    require(statement->kind() == AstNodeKind::CreateCollection, "CREATE COLLECTION kind mismatch");
    const auto * create = static_cast<const CreateCollectionStatement *>(statement.get());
    require(create->collection() == "users", "CREATE COLLECTION name mismatch");
    require(create->columns().size() == 5, "CREATE COLLECTION column count mismatch");
    require(create->columns()[0].primary_key, "PRIMARY KEY constraint mismatch");
    require(create->columns()[1].unique, "UNIQUE constraint mismatch");
    require(create->columns()[1].comment.has_value(), "COMMENT constraint mismatch");
    require(create->columns()[1].type.kind == DataTypeKind::Varchar, "VARCHAR type mismatch");
    require(create->columns()[1].type.parameter.value() == 64, "VARCHAR length mismatch");
    require(create->columns()[2].default_value != nullptr, "DEFAULT literal missing");
    require(create->columns()[4].type.kind == DataTypeKind::Vector, "VECTOR type mismatch");
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

    auto hash_statement = parse_ok("CREATE INDEX idx_name ON users(name) USING HASH;");
    const auto * hash_create = static_cast<const CreateIndexStatement *>(hash_statement.get());
    require(hash_create->method() == CreateIndexMethod::Hash, "CREATE INDEX HASH method mismatch");

    auto default_statement = parse_ok("CREATE INDEX idx_id ON users(id);");
    const auto * default_create = static_cast<const CreateIndexStatement *>(default_statement.get());
    require(default_create->method() == CreateIndexMethod::Default, "CREATE INDEX default method mismatch");
}

void test_parse_drop_show_describe_statements()
{
    auto drop_database = parse_ok("DROP DATABASE IF EXISTS demo;");
    require(drop_database->kind() == AstNodeKind::Drop, "DROP DATABASE kind mismatch");
    const auto * drop_db = static_cast<const DropStatement *>(drop_database.get());
    require(drop_db->object_type() == SchemaObjectType::Database, "DROP DATABASE object type mismatch");
    require(drop_db->if_exists(), "DROP DATABASE IF EXISTS mismatch");

    auto drop_collection = parse_ok("DROP COLLECTION users;");
    const auto * drop_col = static_cast<const DropStatement *>(drop_collection.get());
    require(drop_col->object_type() == SchemaObjectType::Collection, "DROP COLLECTION object type mismatch");
    require(drop_col->name() == "users", "DROP COLLECTION name mismatch");

    auto show_databases = parse_ok("SHOW DATABASES;");
    const auto * show_db = static_cast<const ShowStatement *>(show_databases.get());
    require(show_db->object_type() == SchemaObjectType::Database, "SHOW DATABASES object type mismatch");

    auto show_collections = parse_ok("SHOW COLLECTIONS;");
    const auto * show_col = static_cast<const ShowStatement *>(show_collections.get());
    require(show_col->object_type() == SchemaObjectType::Collection, "SHOW COLLECTIONS object type mismatch");

    auto describe = parse_ok("DESCRIBE users;");
    const auto * describe_statement = static_cast<const DescribeStatement *>(describe.get());
    require(describe_statement->object_type() == SchemaObjectType::Collection, "DESCRIBE object type mismatch");
    require(describe_statement->name() == "users", "DESCRIBE name mismatch");

    auto desc = parse_ok("DESC users;");
    require(desc->kind() == AstNodeKind::Describe, "DESC statement kind mismatch");

    auto describe_collection = parse_ok("DESCRIBE COLLECTION users;");
    require(describe_collection->kind() == AstNodeKind::Describe, "DESCRIBE COLLECTION kind mismatch");
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

    auto columns = parse_ok("SELECT id, users.name, users.* FROM users;");
    const auto * column_select = static_cast<const SelectStatement *>(columns.get());
    require(column_select->select_list().size() == 3, "SELECT qualified list size mismatch");
    require(column_select->select_list()[1]->kind() == AstNodeKind::ColumnReference, "SELECT qualified column mismatch");
    require(column_select->select_list()[2]->kind() == AstNodeKind::Wildcard, "SELECT qualified wildcard mismatch");
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
    require(empty.code == ParserErrorCode::EmptyStatement, "empty input error code mismatch");
    require(empty.message == "Empty statement", "empty input error mismatch");

    auto trailing = parse_error("USE demo extra");
    require(trailing.code == ParserErrorCode::UnexpectedToken, "trailing token error code mismatch");
    require(trailing.location.column == 10, "trailing token location mismatch");

    auto multiple_semicolon = parse_error("USE demo;;");
    require(multiple_semicolon.code == ParserErrorCode::UnexpectedToken, "multiple semicolon error code mismatch");
    require(multiple_semicolon.location.column == 10, "multiple semicolon location mismatch");

    auto missing_name = parse_error("CREATE DATABASE;");
    require(missing_name.code == ParserErrorCode::ExpectedIdentifier, "missing object name error code mismatch");
    require(missing_name.message == "Expected database name", "missing object name error mismatch");

    auto empty_columns = parse_error("CREATE COLLECTION users ();");
    require(empty_columns.code == ParserErrorCode::EmptyList, "empty column list error code mismatch");
    require(empty_columns.message == "Expected at least one column definition", "empty column list error mismatch");

    auto missing_varchar_length = parse_error("CREATE COLLECTION users (name VARCHAR());");
    require(missing_varchar_length.code == ParserErrorCode::ExpectedToken, "VARCHAR missing length error code mismatch");
    require(missing_varchar_length.message == "Expected VARCHAR length", "VARCHAR missing length error mismatch");

    auto missing_vector_dimension = parse_error("CREATE COLLECTION users (embedding VECTOR());");
    require(missing_vector_dimension.code == ParserErrorCode::ExpectedToken, "VECTOR missing dimension error code mismatch");
    require(missing_vector_dimension.message == "Expected VECTOR dimension", "VECTOR missing dimension error mismatch");

    auto default_expression = parse_error("CREATE COLLECTION users (age INTEGER DEFAULT age);");
    require(default_expression.code == ParserErrorCode::ExpectedLiteral, "DEFAULT expression error code mismatch");
    require(default_expression.message == "Expected literal after DEFAULT", "DEFAULT expression error mismatch");

    auto unsupported_as = parse_error("SELECT name AS username FROM users;");
    require(unsupported_as.code == ParserErrorCode::ExpectedToken, "AS unsupported error code mismatch");
    require(unsupported_as.message == "Expected FROM after select list", "AS unsupported error mismatch");

    auto unsupported_group_by = parse_error("SELECT age FROM users GROUP BY age;");
    require(unsupported_group_by.code == ParserErrorCode::UnexpectedToken, "GROUP BY unsupported error code mismatch");
    require(unsupported_group_by.message == "Unexpected token", "GROUP BY unsupported error mismatch");

    auto unsupported_index_method = parse_error("CREATE INDEX idx_age ON users(age) USING gin;");
    require(unsupported_index_method.code == ParserErrorCode::UnsupportedSyntax, "CREATE INDEX method error code mismatch");
    require(unsupported_index_method.message == "Expected HASH or BTREE after USING", "CREATE INDEX method error mismatch");

    auto unsupported_b_tree = parse_error("CREATE INDEX idx_age ON users(age) USING B_TREE;");
    require(unsupported_b_tree.code == ParserErrorCode::UnsupportedSyntax, "CREATE INDEX B_TREE error code mismatch");
    require(unsupported_b_tree.message == "Expected HASH or BTREE after USING", "CREATE INDEX B_TREE error mismatch");

    auto lexical_error = parse_error("SELECT ! FROM users;");
    require(lexical_error.code == ParserErrorCode::LexicalError, "lexical error code mismatch");
}

} // namespace

int main()
{
    try {
        test_parse_use_statement();
        test_parse_create_database_statement();
        test_parse_create_collection_statement();
        test_parse_create_index_statement();
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
