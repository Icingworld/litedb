#pragma once

#include <optional>
#include <string>

#include "core/common/logical_type.hpp"
#include "core/common/value.hpp"

namespace litedb::core::binder::detail
{

// 解析字面量值
[[nodiscard]]
std::optional<common::Value>
parse_literal_value(common::LogicalTypeId type_id, const std::string & text);

} // namespace litedb::core::binder::detail
