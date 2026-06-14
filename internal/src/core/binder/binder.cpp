#include "core/binder/binder.hpp"

#include <algorithm>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/binder/bound/expression/bound_wildcard_expression.hpp"
#include "core/binder/bound/statement/bound_create_collection_statement.hpp"
#include "core/binder/bound/statement/bound_create_database_statement.hpp"
#include "core/binder/bound/statement/bound_create_index_statement.hpp"
#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_delete_statement.hpp"
#include "core/binder/bound/statement/bound_describe_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_collection_statement.hpp"
#include "core/binder/bound/statement/bound_drop_database_statement.hpp"
#include "core/binder/bound/statement/bound_drop_index_statement.hpp"
#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/bound/statement/bound_select_statement.hpp"
#include "core/binder/bound/statement/bound_show_collections_statement.hpp"
#include "core/binder/bound/statement/bound_show_databases_statement.hpp"
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/catalog/catalog_default_expression.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/function/builtin/builtin_functions.hpp"
#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/expression_node.hpp"
// 暂不支持函数调用
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/ast/statement/describe_statement.hpp"
#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/ast/statement/show_statement.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"

namespace litedb::core::binder
{

namespace
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

/**
 * @brief 绑定集合
 * @details 用于临时管理当前数据库 ID 和集合
 */
struct BindingCollection
{
    DatabaseId database_id {0};                               ///< 数据库 ID
    const catalog::CollectionEntry * collection {nullptr};    ///< 集合
};

/**
 * @brief 创建逻辑类型
 * @param id 逻辑类型 ID
 * @param parameter 逻辑类型参数
 * @return 逻辑类型
 */
[[nodiscard]]
LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

/**
 * @brief 创建绑定错误
 * @param code 错误码
 * @param location 错误位置
 * @param message 错误消息
 * @return 绑定错误
 */
[[nodiscard]]
BinderError make_binder_error(BinderErrorCode code, AstNodeLocation location, std::string message)
{
    return BinderError {
        .code = code,
        .location = location,
        .message = std::move(message),
    };
}

/**
 * @brief 判断两个逻辑类型是否相同
 * @param left 左边的逻辑类型
 * @param right 右边的逻辑类型
 * @return 是否相同
 */
[[nodiscard]]
bool same_type(const LogicalType & left, const LogicalType & right)
{
    return left.id == right.id && left.parameter == right.parameter;
}

/**
 * @brief 判断一个逻辑类型是否为数字类型
 * @param value 逻辑类型
 * @return 是否为数字类型
 */
[[nodiscard]]
bool is_numeric(const LogicalType & value)
{
    return value.id == LogicalTypeId::Integer
        || value.id == LogicalTypeId::BigInt
        || value.id == LogicalTypeId::Float
        || value.id == LogicalTypeId::Double;
}

/**
 * @brief 判断一个逻辑类型是否为布尔类型
 * @param value 逻辑类型
 * @return 是否为布尔类型
 */
[[nodiscard]]
bool is_boolean(const LogicalType & value)
{
    return value.id == LogicalTypeId::Boolean;
}

/**
 * @brief 判断一个逻辑类型是否为字符串类型
 * @param value 逻辑类型
 * @return 是否为字符串类型
 */
[[nodiscard]]
bool is_varchar(const LogicalType & value)
{
    return value.id == LogicalTypeId::Varchar;
}

/**
 * @brief 获取逻辑类型的名称
 * @param value 逻辑类型
 * @return 逻辑类型的名称
 */
[[nodiscard]]
std::string type_name(const LogicalType & value)
{
    std::string name;
    switch (value.id) {
    case LogicalTypeId::Null:
        name = "NULL";
        break;
    case LogicalTypeId::Boolean:
        name = "BOOLEAN";
        break;
    case LogicalTypeId::Integer:
        name = "INTEGER";
        break;
    case LogicalTypeId::BigInt:
        name = "BIGINT";
        break;
    case LogicalTypeId::Float:
        name = "FLOAT";
        break;
    case LogicalTypeId::Double:
        name = "DOUBLE";
        break;
    case LogicalTypeId::Varchar:
        name = "VARCHAR";
        break;
    case LogicalTypeId::Vector:
        name = "VECTOR";
        break;
    }

    if (value.parameter.has_value()) {
        name += "(" + std::to_string(value.parameter.value()) + ")";
    }
    return name;
}

/**
 * @brief 获取逻辑类型的数字排名
 * @param value 逻辑类型
 * @return 数字排名
 */
[[nodiscard]]
int numeric_rank(const LogicalType & value)
{
    switch (value.id) {
    case LogicalTypeId::Integer:
        return 1;
    case LogicalTypeId::BigInt:
        return 2;
    case LogicalTypeId::Float:
        return 3;
    case LogicalTypeId::Double:
        return 4;
    default:
        return 0;
    }
}

/**
 * @brief 获取两个逻辑类型的公共数字类型
 * @param left 左边的逻辑类型
 * @param right 右边的逻辑类型
 * @return 公共数字类型
 */
[[nodiscard]]
LogicalType common_numeric_type(const LogicalType & left, const LogicalType & right)
{
    return numeric_rank(left) >= numeric_rank(right) ? left : right;
}

/**
 * @brief 判断一个逻辑类型是否可以转换为另一个逻辑类型
 * @param source 源逻辑类型
 * @param target 目标逻辑类型
 * @return 是否可以转换
 */
[[nodiscard]]
bool can_cast(const LogicalType & source, const LogicalType & target)
{
    if (source.id == LogicalTypeId::Null) {
        return true;
    }

    if (same_type(source, target)) {
        return true;
    }

    if (is_numeric(source) && is_numeric(target)) {
        return numeric_rank(source) <= numeric_rank(target);
    }

    if (source.id == LogicalTypeId::Varchar && target.id == LogicalTypeId::Varchar) {
        return true;
    }

    if (source.id == LogicalTypeId::Vector && target.id == LogicalTypeId::Vector) {
        return !source.parameter.has_value()
            || !target.parameter.has_value()
            || source.parameter.value() == target.parameter.value();
    }

    return false;
}

/**
 * @brief 判断两个逻辑类型是否可以比较
 * @param left 左边的逻辑类型
 * @param right 右边的逻辑类型
 * @param op 操作符
 * @return 是否可以比较
 */
[[nodiscard]]
bool can_compare(const LogicalType & left, const LogicalType & right, TokenType op)
{
    if (is_numeric(left) && is_numeric(right)) {
        return true;
    }

    if (is_varchar(left) && is_varchar(right)) {
        return true;
    }

    if (same_type(left, right)) {
        if (op == TokenType::Equal || op == TokenType::NotEqual) {
            return true;
        }
        return left.id == LogicalTypeId::Varchar
            || left.id == LogicalTypeId::Boolean
            || is_numeric(left);
    }

    return false;
}

/**
 * @brief 如果需要，将表达式转换为目标逻辑类型
 * @param expression 表达式
 * @param target_type 目标逻辑类型
 * @return 转换后的表达式
 */
[[nodiscard]]
std::unique_ptr<BoundExpression> cast_if_needed(std::unique_ptr<BoundExpression> expression, LogicalType target_type)
{
    if (same_type(expression->type(), target_type)) {
        return expression;
    }
    const auto location = expression->location();
    return std::make_unique<BoundCastExpression>(std::move(expression), target_type, location);
}

/**
 * @brief 从列条目创建绑定列
 * @param column 列条目
 * @return 绑定列
 */
[[nodiscard]]
BoundColumn bound_column_from_entry(const catalog::ColumnEntry & column)
{
    return BoundColumn {
        .column_id = column.id(),
        .name = column.name(),
        .type = column.type(),
        .nullable = column.nullable(),
    };
}

/**
 * @brief 将创建索引方法转换为 catalog 的索引类型
 * @param method 创建索引方法
 * @return 索引类型
 */
[[nodiscard]]
catalog::CatalogIndexKind catalog_index_kind(CreateIndexMethod method)
{
    switch (method) {
    case CreateIndexMethod::Hash:
        return catalog::CatalogIndexKind::Hash;
    case CreateIndexMethod::Default:
        [[fallthrough]];
    case CreateIndexMethod::BTree:
        return catalog::CatalogIndexKind::BTree;
    }

    return catalog::CatalogIndexKind::BTree;
}

/**
 * @brief 绑定器工作器
 * @details 真正处理绑定 AST 节点的工作类，隔离了 Binder 的接口和实现
 */
class BinderWorker
{
public:
    BinderWorker(const catalog::CatalogReader & catalog, const SessionContext & session);

public:
    /**
     * @brief 绑定语句
     * @param statement 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_statement(const StatementNode & statement);

private:
    /**
     * @brief 绑定 USE 语句
     * @param statement USE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_use(const UseStatement & statement);

    /**
     * @brief 绑定 CREATE DATABASE 语句
     * @param statement CREATE DATABASE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_create_database(const CreateDatabaseStatement & statement);

    /**
     * @brief 绑定 CREATE COLLECTION 语句
     * @param statement CREATE COLLECTION 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_create_collection(const CreateCollectionStatement & statement);

    /**
     * @brief 绑定 CREATE INDEX 语句
     * @param statement CREATE INDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_create_index(const CreateIndexStatement & statement);

    /**
     * @brief 绑定 CREATE VINDEX 语句
     * @param statement CREATE VINDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_create_vector_index(
        const CreateVectorIndexStatement & statement
    );

    /**
     * @brief 绑定 DROP DATABASE 语句
     * @param statement DROP DATABASE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_drop_database(const DropDatabaseStatement & statement);

    /**
     * @brief 绑定 DROP COLLECTION 语句
     * @param statement DROP COLLECTION 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_drop_collection(const DropCollectionStatement & statement);

    /**
     * @brief 绑定 DROP INDEX 语句
     * @param statement DROP INDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_drop_index(const DropIndexStatement & statement);

    /**
     * @brief 绑定 DROP VINDEX 语句
     * @param statement DROP VINDEX 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_drop_vector_index(
        const DropVectorIndexStatement & statement
    );

    /**
     * @brief 绑定 SHOW 语句
     * @param statement SHOW 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_show(const ShowStatement & statement);

    /**
     * @brief 绑定 DESCRIBE 语句
     * @param statement DESCRIBE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_describe(const DescribeStatement & statement);

    /**
     * @brief 绑定 SELECT 语句
     * @param statement SELECT 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_select(const SelectStatement & statement);

    /**
     * @brief 绑定 INSERT 语句
     * @param statement INSERT 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_insert(const InsertStatement & statement);

    /**
     * @brief 绑定 UPDATE 语句
     * @param statement UPDATE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_update(const UpdateStatement & statement);

    /**
     * @brief 绑定 DELETE 语句
     * @param statement DELETE 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundStatement>, BinderError> bind_delete(const DeleteStatement & statement);

    /**
     * @brief 要求数据库
     * @param location 位置
     * @return 数据库 ID
     */
    [[nodiscard]]
    std::expected<DatabaseId, BinderError> require_database(AstNodeLocation location) const;

    /**
     * @brief 绑定集合
     * @param collection_name 集合名称
     * @param location 位置
     * @return 绑定后的集合
     */
    [[nodiscard]]
    std::expected<BindingCollection, BinderError> bind_collection(
    const std::string & collection_name,
    AstNodeLocation location
    ) const;

    /**
     * @brief 绑定表达式
     * @param expression 表达式
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_expression(const ExpressionNode & expression, const BindingCollection & collection);

    /**
     * @brief 绑定字面量
     * @param expression 字面量
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_literal(const LiteralExpression & expression);

    /**
     * @brief 绑定列引用
     * @param expression 列引用
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_column_reference(
    const ColumnReferenceExpression & expression,
    const BindingCollection & collection
    );

    /**
     * @brief 绑定一元运算符
     * @param expression 一元运算符
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_unary(const UnaryExpression & expression, const BindingCollection & collection);

    /**
     * @brief 绑定二元运算符
     * @param expression 二元运算符
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_binary(const BinaryExpression & expression, const BindingCollection & collection);

    /**
     * @brief 绑定向量
     * @param expression 向量
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_vector(const VectorExpression & expression, const BindingCollection & collection);

    /**
     * @brief 绑定函数
     * @param expression 函数
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_function(const FunctionCallExpression & expression, const BindingCollection & collection);

    /**
     * @brief 绑定 IN
     * @param expression IN
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_in(const InExpression & expression, const BindingCollection & collection);

    /**
     * @brief 绑定 BETWEEN
     * @param expression BETWEEN
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_between(const BetweenExpression & expression, const BindingCollection & collection);

    /**
     * @brief 绑定 LIKE
     * @param expression LIKE
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_like(const LikeExpression & expression, const BindingCollection & collection);

    /**
     * @brief 展开通配符
     * @param expression 通配符
     * @param collection 绑定集合
     * @return 展开后的表达式
     */
    [[nodiscard]]
    std::expected<std::vector<std::unique_ptr<BoundExpression>>, BinderError> expand_wildcard(
    const WildcardExpression & expression,
    const BindingCollection & collection
    );

    /**
     * @brief 绑定默认表达式
     * @param expression 默认表达式
     * @param location 位置
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_default_expression(
    const catalog::CatalogDefaultExpression & expression,
    AstNodeLocation location
    );

    /**
     * @brief 绑定列定义
     * @param columns 列定义列表
     * @param location 位置
     * @return 绑定后的列定义列表
     */
    [[nodiscard]]
    std::expected<std::vector<catalog::ColumnDefinition>, BinderError> bind_column_definitions(
    const ColumnDefinitionList & columns,
    AstNodeLocation location
    );

    /**
     * @brief 绑定数据类型
     * @param data_type 数据类型
     * @param location 位置
     * @return 绑定后的数据类型
     */
    [[nodiscard]]
    std::expected<LogicalType, BinderError> bind_data_type(const DataType & data_type, AstNodeLocation location);

    /**
     * @brief 快照默认表达式
     * @param expression 表达式
     * @return 快照后的默认表达式
     */
    [[nodiscard]]
    std::expected<catalog::CatalogDefaultExpression, BinderError> snapshot_default_expression(
    const ExpressionNode & expression
    );

    const catalog::CatalogReader & catalog_;    ///< 数据库读取器
    const SessionContext & session_;            ///< 会话上下文
};

} // namespace

BinderWorker::BinderWorker(const catalog::CatalogReader & catalog, const SessionContext & session)
    : catalog_(catalog)
    , session_(session)
{
}

/**
* @brief 绑定语句
* @param statement 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_statement(const StatementNode & statement)
{
    // 根据语句类型分发到不同的绑定
    switch (statement.kind()) {
    case AstNodeKind::Use:
        return bind_use(static_cast<const UseStatement &>(statement));
    case AstNodeKind::CreateDatabase:
        return bind_create_database(static_cast<const CreateDatabaseStatement &>(statement));
    case AstNodeKind::CreateCollection:
        return bind_create_collection(static_cast<const CreateCollectionStatement &>(statement));
    case AstNodeKind::CreateIndex:
        return bind_create_index(static_cast<const CreateIndexStatement &>(statement));
    case AstNodeKind::CreateVectorIndex:
        return bind_create_vector_index(static_cast<const CreateVectorIndexStatement &>(statement));
    case AstNodeKind::DropDatabase:
        return bind_drop_database(static_cast<const DropDatabaseStatement &>(statement));
    case AstNodeKind::DropCollection:
        return bind_drop_collection(static_cast<const DropCollectionStatement &>(statement));
    case AstNodeKind::DropIndex:
        return bind_drop_index(static_cast<const DropIndexStatement &>(statement));
    case AstNodeKind::DropVectorIndex:
        return bind_drop_vector_index(static_cast<const DropVectorIndexStatement &>(statement));
    case AstNodeKind::Show:
        return bind_show(static_cast<const ShowStatement &>(statement));
    case AstNodeKind::Describe:
        return bind_describe(static_cast<const DescribeStatement &>(statement));
    case AstNodeKind::Insert:
        return bind_insert(static_cast<const InsertStatement &>(statement));
    case AstNodeKind::Select:
        return bind_select(static_cast<const SelectStatement &>(statement));
    case AstNodeKind::Update:
        return bind_update(static_cast<const UpdateStatement &>(statement));
    case AstNodeKind::Delete:
        return bind_delete(static_cast<const DeleteStatement &>(statement));
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::UnsupportedStatement,
            statement.location(),
            "Unsupported statement"
        ));
    }
}

/**
* @brief 绑定 USE 语句
* @param statement USE 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_use(const UseStatement & statement)
{
    // 查找数据库
    const auto * database = catalog_.find_database(statement.database());
    if (database == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            statement.location(),
            "Database not found: " + statement.database()
        ));
    }

    return std::make_unique<BoundUseStatement>(database->id(), database->name(), statement.location());
}

/**
* @brief 绑定 CREATE DATABASE 语句
* @param statement CREATE DATABASE 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_create_database(const CreateDatabaseStatement & statement)
{
    return std::make_unique<BoundCreateDatabaseStatement>(
        statement.database(),
        statement.if_not_exists(),
        statement.location()
    );
}

/**
* @brief 绑定 CREATE COLLECTION 语句
* @param statement CREATE COLLECTION 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_create_collection(const CreateCollectionStatement & statement)
{
    const auto database_id = require_database(statement.location());
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    auto columns = bind_column_definitions(statement.columns(), statement.location());
    if (!columns.has_value()) [[unlikely]] {
        return std::unexpected(std::move(columns.error()));
    }

    return std::make_unique<BoundCreateCollectionStatement>(
        database_id.value(),
        statement.collection(),
        statement.if_not_exists(),
        std::move(columns.value()),
        statement.location()
    );
}

/**
* @brief 绑定 CREATE INDEX 语句
* @param statement CREATE INDEX 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_create_index(
    const CreateIndexStatement & statement
)
{
    auto collection = bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 查找列
    const auto * column = catalog_.find_column(collection->collection->id(), statement.column_name());
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            statement.location(),
            "Column not found: " + statement.column_name()
        ));
    }

    // 检查列类型是否为向量，不允许在向量类型列上创建普通索引
    if (column->type().id == LogicalTypeId::Vector) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            statement.location(),
            "Scalar index cannot be created on VECTOR column: " + column->name()
        ));
    }

    return std::make_unique<BoundCreateIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        column->id(),
        column->name(),
        statement.index_name(),
        catalog_index_kind(statement.method()),
        false,
        statement.if_not_exists(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_create_vector_index(
    const CreateVectorIndexStatement & statement
)
{
    auto collection = bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    const auto * column = catalog_.find_column(collection->collection->id(), statement.column_name());
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            statement.location(),
            "Column not found: " + statement.column_name()
        ));
    }

    if (column->type().id != LogicalTypeId::Vector || !column->type().parameter.has_value() || column->type().parameter.value() == 0) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            statement.location(),
            "Vector index can only be created on VECTOR(n) column: " + column->name()
        ));
    }

    catalog::CatalogVectorDistanceMetric metric = catalog::CatalogVectorDistanceMetric::L2;
    switch (statement.options().metric) {
    case VectorIndexMetric::Default:
        metric = catalog::CatalogVectorDistanceMetric::L2;
        break;
    case VectorIndexMetric::L2:
        metric = catalog::CatalogVectorDistanceMetric::L2;
        break;
    case VectorIndexMetric::InnerProduct:
        metric = catalog::CatalogVectorDistanceMetric::InnerProduct;
        break;
    case VectorIndexMetric::Cosine:
        metric = catalog::CatalogVectorDistanceMetric::Cosine;
        break;
    }

    return std::make_unique<BoundCreateVectorIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        column->id(),
        column->name(),
        statement.index_name(),
        catalog::CatalogVectorIndexKind::Hnsw,
        metric,
        statement.options().max_neighbors.value_or(16),
        statement.options().ef_construction.value_or(200),
        statement.options().ef_search.value_or(64),
        statement.options().random_seed.value_or(0),
        statement.if_not_exists(),
        statement.location()
    );
}

/**
* @brief 绑定 DROP DATABASE 语句
* @param statement DROP DATABASE 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_drop_database(
    const DropDatabaseStatement & statement
)
{
    const auto * database = catalog_.find_database(statement.database_name());
    if (database == nullptr && !statement.if_exists()) {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            statement.location(),
            "Database not found: " + statement.database_name()
        ));
    }

    return std::make_unique<BoundDropDatabaseStatement>(
        database == nullptr ? std::nullopt : std::optional<DatabaseId>(database->id()),
        statement.database_name(),
        statement.if_exists(),
        statement.location()
    );
}

/**
* @brief 绑定 DROP COLLECTION 语句
* @param statement DROP COLLECTION 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_drop_collection(
    const DropCollectionStatement & statement
)
{
    const auto database_id = require_database(statement.location());
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    const auto * collection = catalog_.find_collection(database_id.value(), statement.collection_name());
    if (collection == nullptr && !statement.if_exists()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            statement.location(),
            "Collection not found: " + statement.collection_name()
        ));
    }

    return std::make_unique<BoundDropCollectionStatement>(
        database_id.value(),
        collection == nullptr ? std::nullopt : std::optional<CollectionId>(collection->id()),
        statement.collection_name(),
        statement.if_exists(),
        statement.location()
    );
}

/**
* @brief 绑定 DROP INDEX 语句
* @param statement DROP INDEX 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_drop_index(
    const DropIndexStatement & statement
)
{
    auto collection = bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 查找索引
    const auto * index = catalog_.find_index(collection->collection->id(), statement.index_name());
    if (index == nullptr && !statement.if_exists()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::IndexNotFound,
            statement.location(),
            "Index not found: " + statement.index_name()
        ));
    }

    return std::make_unique<BoundDropIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        statement.index_name(),
        statement.if_exists(),
        statement.location()
    );
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_drop_vector_index(
    const DropVectorIndexStatement & statement
)
{
    auto collection = bind_collection(statement.collection_name(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    const auto * index = catalog_.find_vector_index(collection->collection->id(), statement.index_name());
    if (index == nullptr && !statement.if_exists()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::IndexNotFound,
            statement.location(),
            "Vector index not found: " + statement.index_name()
        ));
    }

    return std::make_unique<BoundDropVectorIndexStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        statement.index_name(),
        statement.if_exists(),
        statement.location()
    );
}

/**
* @brief 绑定 SHOW 语句
* @param statement SHOW 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_show(const ShowStatement & statement)
{
    if (statement.object_type() == SchemaObjectType::Database) {
        return std::make_unique<BoundShowDatabasesStatement>(statement.location());
    }

    const auto database_id = require_database(statement.location());
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    return std::make_unique<BoundShowCollectionsStatement>(database_id.value(), statement.location());
}

/**
* @brief 绑定 DESCRIBE 语句
* @param statement DESCRIBE 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_describe(const DescribeStatement & statement)
{
    if (statement.object_type() != SchemaObjectType::Collection) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::UnsupportedStatement,
            statement.location(),
            "Only DESCRIBE COLLECTION is supported"
        ));
    }

    const auto database_id = require_database(statement.location());
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    const auto * collection = catalog_.find_collection(database_id.value(), statement.name());
    if (collection == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            statement.location(),
            "Collection not found: " + statement.name()
        ));
    }

    return std::make_unique<BoundDescribeCollectionStatement>(
        database_id.value(),
        collection->id(),
        collection->name(),
        statement.location()
    );
}

/**
* @brief 绑定 SELECT 语句
* @param statement SELECT 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_select(const SelectStatement & statement)
{
    auto collection = bind_collection(statement.collection(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 绑定选择列表
    std::vector<std::unique_ptr<BoundExpression>> projections;
    // 遍历选择列表，binder 不做去重，只负责绑定
    for (const auto & item : statement.select_list()) {
        if (item->kind() == AstNodeKind::Wildcard) {
            // 展开 * 为所有列
            auto expanded = expand_wildcard(static_cast<const WildcardExpression &>(*item), collection.value());
            if (!expanded.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expanded.error()));
            }
            for (auto & expression : expanded.value()) {
                projections.push_back(std::move(expression));
            }
            continue;
        }

        // 不是 * ，绑定列引用
        auto expression = bind_expression(*item, collection.value());
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }
        projections.push_back(std::move(expression.value()));
    }

    // 绑定条件表达式
    std::unique_ptr<BoundExpression> where;
    if (statement.where() != nullptr) {
        auto bound_where = bind_expression(*statement.where(), collection.value());
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean(bound_where.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                statement.where()->location(),
                "WHERE expression must be BOOLEAN"
            ));
        }
        where = std::move(bound_where.value());
    }

    // 绑定排序列表
    std::vector<BoundOrderByItem> order_by;
    for (const auto & item : statement.order_by()) {
        auto expression = bind_expression(*item.expression, collection.value());
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }
        order_by.push_back(BoundOrderByItem {
            .expression = std::move(expression.value()),
            .ascending = item.ascending,
        });
    }

    return std::make_unique<BoundSelectStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        std::move(projections),
        std::move(where),
        std::move(order_by),
        statement.limit(),
        statement.offset(),
        statement.location()
    );
}

/**
* @brief 绑定 INSERT 语句
* @param statement INSERT 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_insert(const InsertStatement & statement)
{
    auto collection = bind_collection(statement.collection(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    const auto catalog_columns = catalog_.list_columns(collection->collection->id());
    std::vector<const catalog::ColumnEntry *> target_columns;
    target_columns.reserve(catalog_columns.size());
    std::vector<std::optional<std::size_t>> source_value_by_target;

    if (statement.columns().empty()) {
        // 没有指定列，使用所有列
        if (statement.values().size() != catalog_columns.size()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidValueCount,
                statement.location(),
                "INSERT value count does not match collection column count"
            ));
        }
        for (std::size_t index = 0; index < catalog_columns.size(); ++index) {
            target_columns.push_back(catalog_columns[index]);
            source_value_by_target.emplace_back(index);
        }
    } else {
        // 指定了列，检查列和值的数量是否匹配
        if (statement.columns().size() != statement.values().size()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidValueCount,
                statement.location(),
                "INSERT column count does not match value count"
            ));
        }

        // 检查列是否重复
        std::unordered_set<std::string> seen_columns;
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            const auto column_key = catalog::normalize_identifier(statement.columns()[index]);
            if (!seen_columns.emplace(column_key).second) {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::DuplicateColumn,
                    statement.location(),
                    "Duplicate INSERT target column: " + statement.columns()[index]
                ));
            }
        }

        // 使用所有列
        for (const auto * column : catalog_columns) {
            target_columns.push_back(column);
            source_value_by_target.emplace_back(std::nullopt);
        }

        // 遍历指定列，绑定列引用
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            const auto * column = catalog_.find_column(collection->collection->id(), statement.columns()[index]);
            if (column == nullptr) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::ColumnNotFound,
                    statement.location(),
                    "Column not found: " + statement.columns()[index]
                ));
            }

            const auto target_it = std::ranges::find(target_columns, column);
            source_value_by_target[static_cast<std::size_t>(target_it - target_columns.begin())] = index;
        }
    }

    // 绑定列和值
    std::vector<BoundColumn> bound_columns;
    std::vector<std::unique_ptr<BoundExpression>> bound_values;
    bound_columns.reserve(target_columns.size());
    bound_values.reserve(target_columns.size());

    for (std::size_t target_index = 0; target_index < target_columns.size(); ++target_index) {
        const auto & column = *target_columns[target_index];
        bound_columns.push_back(bound_column_from_entry(column));

        std::unique_ptr<BoundExpression> value;
        if (source_value_by_target[target_index].has_value()) {
            auto expression = bind_expression(
                *statement.values()[source_value_by_target[target_index].value()],
                collection.value()
            );
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expression.error()));
            }
            value = std::move(expression.value());
        } else if (column.default_expression().has_value()) {
            auto expression = bind_default_expression(column.default_expression().value(), statement.location());
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expression.error()));
            }
            value = std::move(expression.value());
        } else if (column.nullable()) [[likely]] {
            value = std::make_unique<BoundNullExpression>(column.type(), statement.location());
        } else [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                statement.location(),
                "Column requires a value: " + column.name()
            ));
        }

        // 检查值类型是否匹配列类型
        if (!can_cast(value->type(), column.type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value->location(),
                "INSERT value type " + type_name(value->type())
                    + " does not match column " + column.name()
                    + " type " + type_name(column.type())
            ));
        }
        // 检查值是否为 NULL
        if (value->type().id == LogicalTypeId::Null && !column.nullable()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                value->location(),
                "Column cannot be NULL: " + column.name()
            ));
        }

        bound_values.push_back(cast_if_needed(std::move(value), column.type()));
    }

    return std::make_unique<BoundInsertStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        std::move(bound_columns),
        std::move(bound_values),
        statement.location()
    );
}

/**
* @brief 绑定 UPDATE 语句
* @param statement UPDATE 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_update(const UpdateStatement & statement)
{
    auto collection = bind_collection(statement.collection(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    std::vector<BoundAssignment> assignments;
    std::unordered_set<std::string> seen_columns;
    for (const auto & assignment : statement.assignments()) {
        const auto column_key = catalog::normalize_identifier(assignment.column);
        if (!seen_columns.emplace(column_key).second) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DuplicateColumn,
                statement.location(),
                "Duplicate UPDATE target column: " + assignment.column
            ));
        }

        const auto * column = catalog_.find_column(collection->collection->id(), assignment.column);
        if (column == nullptr) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::ColumnNotFound,
                statement.location(),
                "Column not found: " + assignment.column
            ));
        }

        auto value = bind_expression(*assignment.value, collection.value());
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(value.error()));
        }
        if (!can_cast(value.value()->type(), column->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value.value()->location(),
                "UPDATE value type does not match column: " + column->name()
            ));
        }
        if (value.value()->type().id == LogicalTypeId::Null && !column->nullable()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                value.value()->location(),
                "Column cannot be NULL: " + column->name()
            ));
        }

        assignments.push_back(BoundAssignment {
            .column = bound_column_from_entry(*column),
            .value = cast_if_needed(std::move(value.value()), column->type()),
        });
    }

    // 绑定条件表达式
    std::unique_ptr<BoundExpression> where;
    if (statement.where() != nullptr) {
        auto bound_where = bind_expression(*statement.where(), collection.value());
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean(bound_where.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                statement.where()->location(),
                "WHERE expression must be BOOLEAN"
            ));
        }
        where = std::move(bound_where.value());
    }

    return std::make_unique<BoundUpdateStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        std::move(assignments),
        std::move(where),
        statement.location()
    );
}

/**
* @brief 绑定 DELETE 语句
* @param statement DELETE 语句
* @return 绑定后的语句
*/
std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderWorker::bind_delete(const DeleteStatement & statement)
{
    auto collection = bind_collection(statement.collection(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    std::unique_ptr<BoundExpression> where;
    if (statement.where() != nullptr) {
        auto bound_where = bind_expression(*statement.where(), collection.value());
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean(bound_where.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                statement.where()->location(),
                "WHERE expression must be BOOLEAN"
            ));
        }
        where = std::move(bound_where.value());
    }

    return std::make_unique<BoundDeleteStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        std::move(where),
        statement.location()
    );
}

/**
* @brief 要求数据库
* @param location 位置
* @return 数据库 ID
*/
std::expected<DatabaseId, BinderError> BinderWorker::require_database(AstNodeLocation location)const
{
    if (!session_.current_database_id.has_value()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotSelected,
            location,
            "No database selected"
        ));
    }

    if (catalog_.find_database(session_.current_database_id.value()) == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            location,
            "Current database not found"
        ));
    }

    return session_.current_database_id.value();
}

/**
* @brief 绑定集合
* @param collection_name 集合名称
* @param location 位置
* @return 绑定后的集合
*/
std::expected<BindingCollection, BinderError> BinderWorker::bind_collection( const std::string & collection_name, AstNodeLocation location )const
{
    const auto database_id = require_database(location);
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(database_id.error());
    }

    const auto * collection = catalog_.find_collection(database_id.value(), collection_name);
    if (collection == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            location,
            "Collection not found: " + collection_name
        ));
    }

    return BindingCollection {
        .database_id = database_id.value(),
        .collection = collection,
    };
}

/**
* @brief 绑定表达式
* @param expression 表达式
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_expression(const ExpressionNode & expression, const BindingCollection & collection)
{
    // 根据表达式类型分发到不同的绑定
    switch (expression.kind()) {
    case AstNodeKind::Literal:
        return bind_literal(static_cast<const LiteralExpression &>(expression));
    case AstNodeKind::ColumnReference:
        return bind_column_reference(static_cast<const ColumnReferenceExpression &>(expression), collection);
    case AstNodeKind::Unary:
        return bind_unary(static_cast<const UnaryExpression &>(expression), collection);
    case AstNodeKind::Binary:
        return bind_binary(static_cast<const BinaryExpression &>(expression), collection);
    case AstNodeKind::Vector:
        return bind_vector(static_cast<const VectorExpression &>(expression), collection);
    case AstNodeKind::In:
        return bind_in(static_cast<const InExpression &>(expression), collection);
    case AstNodeKind::Between:
        return bind_between(static_cast<const BetweenExpression &>(expression), collection);
    case AstNodeKind::Like:
        return bind_like(static_cast<const LikeExpression &>(expression), collection);
    case AstNodeKind::Wildcard:
        return std::make_unique<BoundWildcardExpression>(
            static_cast<const WildcardExpression &>(expression).qualifier(),
            expression.location()
        );
    case AstNodeKind::FunctionCall:
        return bind_function(static_cast<const FunctionCallExpression &>(expression), collection);
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::UnsupportedExpression,
            expression.location(),
            "Unsupported expression"
        ));
    }
}

/**
* @brief 绑定字面量
* @param expression 字面量
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_literal(const LiteralExpression & expression)
{
    // 根据字面量类型分发到不同的绑定
    switch (expression.literal_type()) {
    case TokenType::Null:
        return std::make_unique<BoundNullExpression>(type(LogicalTypeId::Null), expression.location());
    case TokenType::True:
    case TokenType::False:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Boolean), expression.value(), expression.location());
    case TokenType::IntegerLiteral:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Integer), expression.value(), expression.location());
    case TokenType::FloatLiteral:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Double), expression.value(), expression.location());
    case TokenType::StringLiteral:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Varchar), expression.value(), expression.location());
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "Unsupported literal"
        ));
    }
}

/**
* @brief 绑定列引用
* @param expression 列引用
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_column_reference( const ColumnReferenceExpression & expression, const BindingCollection & collection )
{
    // 检查限定符是否匹配集合
    if (expression.qualifier().has_value()
        && catalog::normalize_identifier(expression.qualifier().value()) != collection.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            expression.location(),
            "Column qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    const auto * column = catalog_.find_column(collection.collection->id(), expression.column());
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            expression.location(),
            "Column not found: " + expression.column()
        ));
    }

    return std::make_unique<BoundColumnRefExpression>(
        collection.database_id,
        collection.collection->id(),
        collection.collection->name(),
        column->id(),
        column->name(),
        column->type(),
        column->nullable(),
        expression.location()
    );
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_function(const FunctionCallExpression & expression, const BindingCollection & collection)
{
    std::vector<std::unique_ptr<BoundExpression>> arguments;
    std::vector<LogicalType> argument_types;
    arguments.reserve(expression.arguments().size());
    argument_types.reserve(expression.arguments().size());

    for (const auto & argument : expression.arguments()) {
        auto bound_argument = bind_expression(*argument, collection);
        if (!bound_argument.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_argument.error()));
        }
        argument_types.push_back(bound_argument.value()->type());
        arguments.push_back(std::move(bound_argument.value()));
    }

    auto registry = function::builtin::make_builtin_function_registry();
    auto binding = registry.bind_scalar(expression.name(), argument_types);
    if (!binding.has_value()) [[unlikely]] {
        const auto found = registry.find(expression.name());
        return std::unexpected(make_binder_error(
            found == nullptr ? BinderErrorCode::UnsupportedExpression : BinderErrorCode::InvalidType,
            expression.location(),
            found == nullptr ? "Unknown function: " + expression.name() : "Function arguments do not match any overload: " + expression.name()
        ));
    }

    if ((function::normalize_function_name(expression.name()) == "l2_distance"
        || function::normalize_function_name(expression.name()) == "cosine_distance"
        || function::normalize_function_name(expression.name()) == "inner_product")
        && argument_types.size() == 2
        && argument_types[0].id == LogicalTypeId::Vector
        && argument_types[1].id == LogicalTypeId::Vector
        && argument_types[0].parameter.has_value()
        && argument_types[1].parameter.has_value()
        && argument_types[0].parameter.value() != argument_types[1].parameter.value()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "Vector function arguments must have the same dimension"
        ));
    }

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto signature_index = std::min(index, binding->signature.argument_types.size() - 1);
        const auto & target_type = binding->signature.argument_types[signature_index];
        if (!can_cast(arguments[index]->type(), target_type)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                arguments[index]->location(),
                "Function argument type " + type_name(arguments[index]->type())
                    + " cannot be cast to " + type_name(target_type)
            ));
        }
        if (!(arguments[index]->type().id == LogicalTypeId::Vector && target_type.id == LogicalTypeId::Vector && !target_type.parameter.has_value())) {
            arguments[index] = cast_if_needed(std::move(arguments[index]), target_type);
        }
    }

    return std::make_unique<BoundFunctionExpression>(
        expression.name(),
        std::move(binding->function),
        std::move(binding->signature),
        std::move(arguments),
        binding->signature.return_type,
        expression.location()
    );
}

/**
* @brief 绑定一元运算符
* @param expression 一元运算符
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_unary(const UnaryExpression & expression, const BindingCollection & collection)
{
    auto operand = bind_expression(expression.operand(), collection);
    if (!operand.has_value()) [[unlikely]] {
        return std::unexpected(std::move(operand.error()));
    }

    if (expression.op() == TokenType::Not) {
        if (!is_boolean(operand.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "NOT operand must be BOOLEAN"
            ));
        }
        return std::make_unique<BoundUnaryExpression>(
            expression.op(),
            std::move(operand.value()),
            type(LogicalTypeId::Boolean),
            expression.location()
        );
    }

    if ((expression.op() == TokenType::Plus || expression.op() == TokenType::Minus)
        && is_numeric(operand.value()->type())) {
        const auto result_type = operand.value()->type();
        return std::make_unique<BoundUnaryExpression>(
            expression.op(),
            std::move(operand.value()),
            result_type,
            expression.location()
        );
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::InvalidType,
        expression.location(),
        "Invalid unary operand type"
    ));
}

/**
* @brief 绑定二元运算符
* @param expression 二元运算符
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_binary(const BinaryExpression & expression, const BindingCollection & collection)
{
    auto left = bind_expression(expression.left(), collection);
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    auto right = bind_expression(expression.right(), collection);
    if (!right.has_value()) [[unlikely]] {
        return std::unexpected(std::move(right.error()));
    }

    const auto op = expression.op();
    if (op == TokenType::And || op == TokenType::Or) {
        if (!is_boolean(left.value()->type()) || !is_boolean(right.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Logical operands must be BOOLEAN"
            ));
        }

        return std::make_unique<BoundBinaryExpression>(
            std::move(left.value()),
            op,
            std::move(right.value()),
            type(LogicalTypeId::Boolean),
            expression.location()
        );
    }

    if (op == TokenType::Plus || op == TokenType::Minus || op == TokenType::Star
        || op == TokenType::Slash || op == TokenType::Modulo) {
        if (!is_numeric(left.value()->type()) || !is_numeric(right.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Arithmetic operands must be numeric"
            ));
        }
        const auto result_type = common_numeric_type(left.value()->type(), right.value()->type());
        return std::make_unique<BoundBinaryExpression>(
            cast_if_needed(std::move(left.value()), result_type),
            op,
            cast_if_needed(std::move(right.value()), result_type),
            result_type,
            expression.location()
        );
    }

    if (op == TokenType::Equal || op == TokenType::NotEqual || op == TokenType::LessThan
        || op == TokenType::LessEqual || op == TokenType::GreaterThan || op == TokenType::GreaterEqual) {
        if (!can_compare(left.value()->type(), right.value()->type(), op)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Comparison operands are not compatible"
            ));
        }
        if (is_numeric(left.value()->type()) && is_numeric(right.value()->type())) {
            const auto common_type = common_numeric_type(left.value()->type(), right.value()->type());
            left = cast_if_needed(std::move(left.value()), common_type);
            right = cast_if_needed(std::move(right.value()), common_type);
        }
        return std::make_unique<BoundBinaryExpression>(
            std::move(left.value()),
            op,
            std::move(right.value()),
            type(LogicalTypeId::Boolean),
            expression.location()
        );
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::UnsupportedExpression,
        expression.location(),
        "Unsupported binary operator"
    ));
}

/**
* @brief 绑定向量
* @param expression 向量
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_vector(const VectorExpression & expression, const BindingCollection & collection)
{
    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.reserve(expression.elements().size());

    for (const auto & element : expression.elements()) {
        auto bound_element = bind_expression(*element, collection);
        if (!bound_element.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_element.error()));
        }
        if (!is_numeric(bound_element.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                element->location(),
                "Vector elements must be numeric"
            ));
        }
        elements.push_back(cast_if_needed(std::move(bound_element.value()), type(LogicalTypeId::Double)));
    }

    return std::make_unique<BoundVectorExpression>(
        std::move(elements),
        type(LogicalTypeId::Vector, expression.elements().size()),
        expression.location()
    );
}

/**
* @brief 绑定 IN
* @param expression IN
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_in(const InExpression & expression, const BindingCollection & collection)
{
    auto target = bind_expression(expression.expression(), collection);
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }

    std::vector<std::unique_ptr<BoundExpression>> values;
    for (const auto & value : expression.values()) {
        auto bound_value = bind_expression(*value, collection);
        if (!bound_value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_value.error()));
        }
        if (!can_compare(target.value()->type(), bound_value.value()->type(), TokenType::Equal)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value->location(),
                "IN value is not comparable with target expression"
            ));
        }
        values.push_back(std::move(bound_value.value()));
    }

    return std::make_unique<BoundInExpression>(std::move(target.value()), std::move(values), expression.location());
}

/**
* @brief 绑定 BETWEEN
* @param expression BETWEEN
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_between(const BetweenExpression & expression, const BindingCollection & collection)
{
    auto target = bind_expression(expression.expression(), collection);
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }
    auto lower = bind_expression(expression.lower(), collection);
    if (!lower.has_value()) [[unlikely]] {
        return std::unexpected(std::move(lower.error()));
    }
    auto upper = bind_expression(expression.upper(), collection);
    if (!upper.has_value()) [[unlikely]] {
        return std::unexpected(std::move(upper.error()));
    }

    if (!can_compare(target.value()->type(), lower.value()->type(), TokenType::GreaterEqual)
        || !can_compare(target.value()->type(), upper.value()->type(), TokenType::LessEqual)) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "BETWEEN bounds are not comparable with target expression"
        ));
    }

    return std::make_unique<BoundBetweenExpression>(
        std::move(target.value()),
        std::move(lower.value()),
        std::move(upper.value()),
        expression.location()
    );
}

/**
* @brief 绑定 LIKE
* @param expression LIKE
* @param collection 绑定集合
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_like(const LikeExpression & expression, const BindingCollection & collection)
{
    auto target = bind_expression(expression.expression(), collection);
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }
    auto pattern = bind_expression(expression.pattern(), collection);
    if (!pattern.has_value()) [[unlikely]] {
        return std::unexpected(std::move(pattern.error()));
    }

    if (!is_varchar(target.value()->type()) || !is_varchar(pattern.value()->type())) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "LIKE operands must be VARCHAR"
        ));
    }

    return std::make_unique<BoundLikeExpression>(
        std::move(target.value()),
        std::move(pattern.value()),
        expression.location()
    );
}

/**
* @brief 展开通配符
* @param expression 通配符
* @param collection 绑定集合
* @return 展开后的表达式
*/
std::expected<std::vector<std::unique_ptr<BoundExpression>>, BinderError> BinderWorker::expand_wildcard( const WildcardExpression & expression, const BindingCollection & collection )
{
    if (expression.qualifier().has_value()
        && catalog::normalize_identifier(expression.qualifier().value()) != collection.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            expression.location(),
            "Wildcard qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    std::vector<std::unique_ptr<BoundExpression>> expressions;
    for (const auto * column : catalog_.list_columns(collection.collection->id())) {
        expressions.push_back(std::make_unique<BoundColumnRefExpression>(
            collection.database_id,
            collection.collection->id(),
            collection.collection->name(),
            column->id(),
            column->name(),
            column->type(),
            column->nullable(),
            expression.location()
        ));
    }
    return expressions;
}

/**
* @brief 绑定默认表达式
* @param expression 默认表达式
* @param location 位置
* @return 绑定后的表达式
*/
std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorker::bind_default_expression( const catalog::CatalogDefaultExpression & expression, AstNodeLocation location )
{
    if (expression.kind == catalog::CatalogDefaultExpressionKind::Vector) {
        std::vector<std::unique_ptr<BoundExpression>> elements;
        elements.reserve(expression.elements.size());
        for (const auto & element : expression.elements) {
            auto bound_element = bind_default_expression(element, location);
            if (!bound_element.has_value()) [[unlikely]] {
                return std::unexpected(std::move(bound_element.error()));
            }
            if (!is_numeric(bound_element.value()->type())) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::InvalidType,
                    location,
                    "Vector default elements must be numeric"
                ));
            }
            elements.push_back(cast_if_needed(std::move(bound_element.value()), type(LogicalTypeId::Double)));
        }
        return std::make_unique<BoundVectorExpression>(
            std::move(elements),
            type(LogicalTypeId::Vector, expression.elements.size()),
            location
        );
    }

    switch (expression.literal_kind) {
    case catalog::CatalogDefaultLiteralKind::Null:
        return std::make_unique<BoundNullExpression>(type(LogicalTypeId::Null), location);
    case catalog::CatalogDefaultLiteralKind::Boolean:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Boolean), expression.value, location);
    case catalog::CatalogDefaultLiteralKind::Integer:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Integer), expression.value, location);
    case catalog::CatalogDefaultLiteralKind::Float:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Double), expression.value, location);
    case catalog::CatalogDefaultLiteralKind::String:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Varchar), expression.value, location);
    }

    [[unlikely]] return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "Unsupported default expression"));
}

/**
* @brief 绑定列定义
* @param columns 列定义列表
* @param location 位置
* @return 绑定后的列定义列表
*/
std::expected<std::vector<catalog::ColumnDefinition>, BinderError> BinderWorker::bind_column_definitions( const ColumnDefinitionList & columns, AstNodeLocation location )
{
    std::unordered_set<std::string> seen_columns;
    bool has_primary_key = false;
    std::vector<catalog::ColumnDefinition> result;
    result.reserve(columns.size());

    for (const auto & column : columns) {
        if (!seen_columns.emplace(catalog::normalize_identifier(column.name)).second) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DuplicateColumn,
                location,
                "Duplicate column: " + column.name
            ));
        }
        if (column.primary_key) {
            if (has_primary_key) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::DuplicatePrimaryKey,
                    location,
                    "Collection cannot have multiple primary keys"
                ));
            }
            has_primary_key = true;
        }

        auto logical_type = bind_data_type(column.type, location);
        if (!logical_type.has_value()) [[unlikely]] {
            return std::unexpected(std::move(logical_type.error()));
        }

        std::optional<catalog::CatalogDefaultExpression> default_expression;
        if (column.default_value != nullptr) {
            auto default_snapshot = snapshot_default_expression(*column.default_value);
            if (!default_snapshot.has_value()) [[unlikely]] {
                return std::unexpected(std::move(default_snapshot.error()));
            }
            default_expression = std::move(default_snapshot.value());

            auto bound_default = bind_default_expression(default_expression.value(), column.default_value->location());
            if (!bound_default.has_value()) [[unlikely]] {
                return std::unexpected(std::move(bound_default.error()));
            }
            if (!can_cast(bound_default.value()->type(), logical_type.value())) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::InvalidType,
                    column.default_value->location(),
                    "DEFAULT value type " + type_name(bound_default.value()->type())
                        + " does not match column " + column.name
                        + " type " + type_name(logical_type.value())
                ));
            }
            if (bound_default.value()->type().id == LogicalTypeId::Null && column.primary_key) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::NotNullable,
                    column.default_value->location(),
                    "PRIMARY KEY column cannot default to NULL: " + column.name
                ));
            }
        }

        result.push_back(catalog::ColumnDefinition {
            .name = column.name,
            .type = logical_type.value(),
            .primary_key = column.primary_key,
            .unique = column.unique,
            .nullable = !column.primary_key,
            .default_expression = std::move(default_expression),
            .comment = column.comment,
        });
    }

    return result;
}

/**
* @brief 绑定数据类型
* @param data_type 数据类型
* @param location 位置
* @return 绑定后的数据类型
*/
std::expected<LogicalType, BinderError> BinderWorker::bind_data_type(const DataType & data_type, AstNodeLocation location)
{
    switch (data_type.kind) {
    case DataTypeKind::Integer:
        return type(LogicalTypeId::Integer);
    case DataTypeKind::BigInt:
        return type(LogicalTypeId::BigInt);
    case DataTypeKind::Float:
        return type(LogicalTypeId::Float);
    case DataTypeKind::Double:
        return type(LogicalTypeId::Double);
    case DataTypeKind::Boolean:
        return type(LogicalTypeId::Boolean);
    case DataTypeKind::Varchar:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "VARCHAR length must be positive"));
        }
        return type(LogicalTypeId::Varchar, data_type.parameter);
    case DataTypeKind::Vector:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "VECTOR dimension must be positive"));
        }
        return type(LogicalTypeId::Vector, data_type.parameter);
    }
    [[unlikely]] return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "Unsupported data type"));
}

/**
* @brief 快照默认表达式
* @param expression 表达式
* @return 快照后的默认表达式
*/
std::expected<catalog::CatalogDefaultExpression, BinderError> BinderWorker::snapshot_default_expression( const ExpressionNode & expression )
{
    if (expression.kind() == AstNodeKind::Literal) {
        const auto & literal = static_cast<const LiteralExpression &>(expression);
        switch (literal.literal_type()) {
        case TokenType::Null:
            return catalog::CatalogDefaultExpression::null_literal();
        case TokenType::True:
            [[fallthrough]];
        case TokenType::False:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::Boolean,
                literal.value()
            );
        case TokenType::IntegerLiteral:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::Integer,
                literal.value()
            );
        case TokenType::FloatLiteral:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::Float,
                literal.value()
            );
        case TokenType::StringLiteral:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::String,
                literal.value()
            );
        default:
            break;
        }
    }

    if (expression.kind() == AstNodeKind::Vector) {
        const auto & vector = static_cast<const VectorExpression &>(expression);
        std::vector<catalog::CatalogDefaultExpression> elements;
        elements.reserve(vector.elements().size());
        for (const auto & element : vector.elements()) {
            auto snapshot = snapshot_default_expression(*element);
            if (!snapshot.has_value()) [[unlikely]] {
                return std::unexpected(std::move(snapshot.error()));
            }
            elements.push_back(std::move(snapshot.value()));
        }
        return catalog::CatalogDefaultExpression::vector(std::move(elements));
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::InvalidType,
        expression.location(),
        "Unsupported default expression"
    ));
}

Binder::Binder(const catalog::CatalogReader & catalog, const SessionContext & session) noexcept
    : catalog_(catalog)
    , session_(session)
{
}

std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> Binder::bind(
    const parser::ast::StatementNode & statement
) const
{
    return BinderWorker(catalog_, session_).bind_statement(statement);
}

} // namespace litedb::core::binder
