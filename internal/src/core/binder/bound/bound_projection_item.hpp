#pragma once

#include <memory>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

// 投影项
struct BoundProjectionItem
{
    std::unique_ptr<BoundExpression> expression;
    std::string output_name;
};

} // namespace litedb::core::binder::bound
