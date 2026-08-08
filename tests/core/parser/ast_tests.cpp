#include "core/common/logical_type.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

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

void test_expression_nodes()
{
    auto left = std::make_unique<ColumnReferenceExpression>(
        std::nullopt,
        "age",
        AstNodeLocation {1, 7}
    );
    auto right = std::make_unique<LiteralExpression>(TokenType::IntegerLiteral, "18", AstNodeLocation {1, 14});
    BinaryExpression expression(std::move(left), TokenType::GreaterEqual, std::move(right), AstNodeLocation {1, 7});

    require(expression.kind() == AstNodeKind::Binary, "binary expression kind mismatch");
    require(expression.location().line == 1, "binary expression line mismatch");
    require(expression.location().column == 7, "binary expression column mismatch");
    require(expression.left().kind() == AstNodeKind::ColumnReference, "left expression kind mismatch");
    require(expression.right().kind() == AstNodeKind::Literal, "right expression kind mismatch");
}

void test_statement_nodes()
{
    std::vector<std::unique_ptr<ExpressionNode>> select_list;
    select_list.push_back(std::make_unique<ColumnReferenceExpression>(
        std::nullopt,
        "name",
        AstNodeLocation {1, 8}
    ));

    SelectStatement statement(
        std::move(select_list),
        "users",
        nullptr,
        {},
        10,
        {},
        AstNodeLocation {1, 1}
    );

    require(statement.kind() == AstNodeKind::Select, "select statement kind mismatch");
    require(statement.location().line == 1, "select statement line mismatch");
    require(statement.collection_name() == "users", "select collection mismatch");
    require(statement.select_list().size() == 1, "select list size mismatch");
    require(!statement.where().has_value(), "select where should be empty");
    require(statement.limit().has_value() && statement.limit().value() == 10, "select limit mismatch");
}

void test_create_database_statement()
{
    CreateDatabaseStatement statement("demo", true, AstNodeLocation {1, 1});

    require(statement.kind() == AstNodeKind::CreateDatabase, "create database kind mismatch");
    require(statement.database_name() == "demo", "create database name mismatch");
    require(statement.if_not_exists(), "create database if-not-exists mismatch");
}

void test_create_collection_statement()
{
    auto id = std::make_unique<ColumnDefinitionSyntax>();
    id->name = "id";
    id->type = LogicalType {LogicalTypeId::BigInt, std::nullopt};

    auto name = std::make_unique<ColumnDefinitionSyntax>();
    name->name = "name";
    name->type = LogicalType {LogicalTypeId::Varchar, 64};

    auto age = std::make_unique<ColumnDefinitionSyntax>();
    age->name = "age";
    age->type = LogicalType {LogicalTypeId::Integer, std::nullopt};
    age->default_value = std::make_unique<LiteralExpression>(
        TokenType::IntegerLiteral,
        "0",
        AstNodeLocation {4, 25}
    );

    auto embedding = std::make_unique<ColumnDefinitionSyntax>();
    embedding->name = "embedding";
    embedding->type = LogicalType {LogicalTypeId::Vector, 128};

    ColumnDefinitionSyntaxList columns;
    columns.push_back(std::move(id));
    columns.push_back(std::move(name));
    columns.push_back(std::move(age));
    columns.push_back(std::move(embedding));

    CreateCollectionStatement statement("users", false, std::move(columns), "user collection", AstNodeLocation {1, 1});

    require(statement.kind() == AstNodeKind::CreateCollection, "create collection kind mismatch");
    require(statement.collection_name() == "users", "create collection name mismatch");
    require(!statement.if_not_exists(), "create collection if-not-exists mismatch");
    require(statement.columns().size() == 4, "create collection columns size mismatch");
    require(statement.comment().has_value(), "create collection comment missing");
    require(statement.comment().value() == "user collection", "create collection comment mismatch");
    require(statement.columns()[1]->type.parameter.has_value(), "varchar parameter should exist");
    require(statement.columns()[1]->type.parameter.value() == 64, "varchar parameter mismatch");
    require(statement.columns()[2]->default_value != nullptr, "default value should exist");
    require(statement.columns()[3]->type.id == LogicalTypeId::Vector, "vector column type mismatch");
    require(statement.columns()[3]->type.parameter.value() == 128, "vector dimension mismatch");
}

void test_schema_object_type_statements()
{
    DropDatabaseStatement drop_database("demo", true, AstNodeLocation {1, 1});
    require(drop_database.kind() == AstNodeKind::DropDatabase, "drop database statement kind mismatch");
    require(drop_database.database_name() == "demo", "drop database name mismatch");
    require(drop_database.if_exists(), "drop database if-exists mismatch");

    DropCollectionStatement drop_collection("users", true, AstNodeLocation {1, 1});
    require(drop_collection.kind() == AstNodeKind::DropCollection, "drop collection statement kind mismatch");
    require(drop_collection.collection_name() == "users", "drop collection name mismatch");
    require(drop_collection.if_exists(), "drop collection if-exists mismatch");

    DropIndexStatement drop_index("idx_age", "users", true, AstNodeLocation {1, 1});
    require(drop_index.kind() == AstNodeKind::DropIndex, "drop index statement kind mismatch");
    require(drop_index.index_name() == "idx_age", "drop index name mismatch");
    require(drop_index.collection_name() == "users", "drop index collection mismatch");
    require(drop_index.if_exists(), "drop index if-exists mismatch");
}

} // namespace

int main()
{
    try {
        test_expression_nodes();
        test_statement_nodes();
        test_create_database_statement();
        test_create_collection_statement();
        test_schema_object_type_statements();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
