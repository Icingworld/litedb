#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "core/binder/binder_error.hpp"
#include "core/binder/binder_context.hpp"
#include "core/catalog/catalog_default_expression.hpp"
#include "core/catalog/catalog_writer.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/parser/ast/ast_node.hpp"
#include "core/parser/ast/schema.hpp"

namespace litedb::core::parser::ast
{

class BetweenExpression;
class BinaryExpression;
class ColumnReferenceExpression;
class ExpressionNode;
class FunctionCallExpression;
class InExpression;
class LikeExpression;
class LiteralExpression;
class UnaryExpression;
class VectorExpression;
class WildcardExpression;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundExpression;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

/**
 * @brief 绑定集合
 */
struct BindingCollection
{
    common::DatabaseId database_id {0};                     ///< 数据库 ID
    const catalog::CollectionEntry * collection {nullptr};  ///< 集合
};

/**
 * @brief 绑定工作器助手
 */
class BinderWorkerHelper
{
public:
    explicit BinderWorkerHelper(BinderContext & context);

public:
    /**
     * @brief 要求数据库
     * @param location 位置
     * @return 数据库 ID
     */
    [[nodiscard]]
    std::expected<common::DatabaseId, BinderError> require_database(parser::ast::AstNodeLocation location) const;

    /**
     * @brief 绑定集合
     * @param collection_name 集合名称
     * @param location 位置
     * @return 绑定后的集合
     */
    [[nodiscard]]
    std::expected<BindingCollection, BinderError> bind_collection(
        const std::string & collection_name,
        parser::ast::AstNodeLocation location
    ) const;

    /**
     * @brief 绑定表达式
     * @param expression 表达式
     * @param collection 绑定集合
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_expression(
        const parser::ast::ExpressionNode & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 展开通配符表达式
     * @param expression 通配符表达式
     * @param collection 绑定集合
     * @return 展开后的表达式
     */
    [[nodiscard]]
    std::expected<std::vector<std::unique_ptr<bound::BoundExpression>>, BinderError> expand_wildcard(
        const parser::ast::WildcardExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定默认表达式
     * @param expression 默认表达式
     * @param location 位置
     * @return 绑定后的表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_default_expression(
        const catalog::CatalogDefaultExpression & expression,
        parser::ast::AstNodeLocation location
    );

    /**
     * @brief 绑定列定义
     * @param columns 列定义列表
     * @param location 位置
     * @return 绑定后的列定义列表
     */
    [[nodiscard]]
    std::expected<std::vector<catalog::ColumnDefinition>, BinderError> bind_column_definitions(
        const parser::ast::ColumnDefinitionList & columns,
        parser::ast::AstNodeLocation location
    );

    /**
     * @brief 绑定数据类型
     * @param data_type 数据类型
     * @param location 位置
     * @return 绑定后的数据类型
     */
    [[nodiscard]]
    std::expected<common::LogicalType, BinderError> bind_data_type(
        const parser::ast::DataType & data_type,
        parser::ast::AstNodeLocation location
    );

    /**
     * @brief 快照默认表达式
     * @param expression 默认表达式
     * @return 快照后的默认表达式
     */
    [[nodiscard]]
    std::expected<catalog::CatalogDefaultExpression, BinderError> snapshot_default_expression(
        const parser::ast::ExpressionNode & expression
    );

private:
    [[nodiscard]]
    /**
     * @brief 绑定字面量表达式
     * @param expression 字面量表达式
     * @return 绑定后的字面量表达式
     */
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_literal(
        const parser::ast::LiteralExpression & expression
    );

    /**
     * @brief 绑定列引用表达式
     * @param expression 列引用表达式
     * @param collection 绑定集合
     * @return 绑定后的列引用表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_column_reference(
        const parser::ast::ColumnReferenceExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定一元表达式
     * @param expression 一元表达式
     * @param collection 绑定集合
     * @return 绑定后的一元表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_unary(
        const parser::ast::UnaryExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定二元表达式
     * @param expression 二元表达式
     * @param collection 绑定集合
     * @return 绑定后的二元表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_binary(
        const parser::ast::BinaryExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定向量表达式
     * @param expression 向量表达式
     * @param collection 绑定集合
     * @return 绑定后的向量表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_vector(
        const parser::ast::VectorExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定函数表达式
     * @param expression 函数表达式
     * @param collection 绑定集合
     * @return 绑定后的函数表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_function(
        const parser::ast::FunctionCallExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定包含表达式
     * @param expression 包含表达式
     * @param collection 绑定集合
     * @return 绑定后的包含表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_in(
        const parser::ast::InExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定范围表达式
     * @param expression 范围表达式
     * @param collection 绑定集合
     * @return 绑定后的范围表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_between(
        const parser::ast::BetweenExpression & expression,
        const BindingCollection & collection
    );

    /**
     * @brief 绑定模糊匹配表达式
     * @param expression 模糊匹配表达式
     * @param collection 绑定集合
     * @return 绑定后的模糊匹配表达式
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_like(
        const parser::ast::LikeExpression & expression,
        const BindingCollection & collection
    );

private:
    BinderContext & context_;        ///< 绑定上下文
};

} // namespace litedb::core::binder
