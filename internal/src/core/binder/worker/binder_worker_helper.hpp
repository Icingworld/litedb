#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_error.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/parser/ast/column_definition.hpp"
#include "core/schema/default_expression.hpp"

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

// 绑定集合
struct BindingCollection
{
    common::DatabaseId database_id {0};
    const meta::entry::CollectionEntry * collection {nullptr};
};

// 绑定工作器助手
class BinderWorkerHelper
{
public:
    explicit BinderWorkerHelper(const BinderContext & context);

public:
    // 获取当前数据库
    [[nodiscard]]
    std::expected<common::DatabaseId, BinderError> require_database() const;

    // 绑定集合
    [[nodiscard]]
    std::expected<BindingCollection, BinderError> bind_collection(
        const std::string & collection_name
    ) const;

    // 绑定表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_expression(
        const parser::ast::ExpressionNode & expression,
        const BindingCollection & collection
    ) const;

    // 展开通配符表达式
    [[nodiscard]]
    std::expected<std::vector<std::unique_ptr<bound::BoundExpression>>, BinderError>
    expand_wildcard(
        const parser::ast::WildcardExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定默认表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_default_expression(
        const schema::DefaultExpression & expression
    ) const;

    // 绑定列定义
    [[nodiscard]]
    std::expected<std::vector<meta::ColumnDefinition>, BinderError> bind_column_definitions(
        const std::vector<std::unique_ptr<parser::ast::ColumnDefinitionSyntax>> & columns
    ) const;

    // 验证声明的数据类型
    [[nodiscard]]
    std::expected<common::LogicalType, BinderError> validate_data_type(
        const common::LogicalType & data_type
    ) const;

    // 快照默认表达式
    [[nodiscard]]
    std::expected<schema::DefaultExpression, BinderError> snapshot_default_expression(
        const parser::ast::ExpressionNode & expression
    ) const;

private:
    // 绑定字面量表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_literal(
        const parser::ast::LiteralExpression & expression
    ) const;

    // 绑定列引用表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_column_reference(
        const parser::ast::ColumnReferenceExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定一元表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_unary(
        const parser::ast::UnaryExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定二元表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_binary(
        const parser::ast::BinaryExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定向量表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_vector(
        const parser::ast::VectorExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定函数表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_function(
        const parser::ast::FunctionCallExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定包含表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_in(
        const parser::ast::InExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定范围表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_between(
        const parser::ast::BetweenExpression & expression,
        const BindingCollection & collection
    ) const;

    // 绑定模糊匹配表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_like(
        const parser::ast::LikeExpression & expression,
        const BindingCollection & collection
    ) const;

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder
