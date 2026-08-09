#pragma once

#include <expected>
#include <memory>
#include <vector>

#include "core/binder/binder_error.hpp"
#include "core/meta/meta_request.hpp"
#include "core/parser/ast/column_definition.hpp"

namespace litedb::core::binder::detail
{

// 绑定列定义
[[nodiscard]]
std::expected<std::vector<meta::ColumnDefinition>, BinderError> bind_column_definitions(
    const std::vector<std::unique_ptr<parser::ast::ColumnDefinitionSyntax>> & columns
);

} // namespace litedb::core::binder::detail
