#include "core/common/value.hpp"

#include <utility>

namespace litedb::core::common
{

Value::Value() = default;

Value::Value(ValueData data)
    : data_(std::move(data))
{
}

Value Value::null()
{
    return Value {};
}

bool Value::is_null() const noexcept
{
    return std::holds_alternative<NullValue>(data_);
}

const ValueData & Value::data() const noexcept
{
    return data_;
}

bool Value::matches_type(const LogicalType & type) const noexcept
{
    if (is_null()) {
        return true;
    }

    switch (type.id) {
    case LogicalTypeId::Null:
        return is_null();
    case LogicalTypeId::Boolean:
        return std::holds_alternative<bool>(data_);
    case LogicalTypeId::Integer:
        return std::holds_alternative<std::int32_t>(data_);
    case LogicalTypeId::BigInt:
        return std::holds_alternative<std::int64_t>(data_);
    case LogicalTypeId::Float:
        return std::holds_alternative<float>(data_);
    case LogicalTypeId::Double:
        return std::holds_alternative<double>(data_);
    case LogicalTypeId::Varchar:
        return std::holds_alternative<std::string>(data_);
    case LogicalTypeId::Vector:
        return std::holds_alternative<VectorValue>(data_);
    }

    return false;
}

} // namespace litedb::core::common
