#include "core/schema/value.hpp"

namespace litedb::core::schema
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

bool Value::matches_type(const common::LogicalType & type) const noexcept
{
    if (is_null()) {
        return true;
    }

    switch (type.id) {
    case common::LogicalTypeId::Null:
        return is_null();
    case common::LogicalTypeId::Boolean:
        return std::holds_alternative<bool>(data_);
    case common::LogicalTypeId::Integer:
        return std::holds_alternative<std::int32_t>(data_);
    case common::LogicalTypeId::BigInt:
        return std::holds_alternative<std::int64_t>(data_);
    case common::LogicalTypeId::Float:
        return std::holds_alternative<float>(data_);
    case common::LogicalTypeId::Double:
        return std::holds_alternative<double>(data_);
    case common::LogicalTypeId::Varchar:
        return std::holds_alternative<std::string>(data_);
    case common::LogicalTypeId::Vector:
        return std::holds_alternative<VectorValue>(data_);
    }

    return false;
}

} // namespace litedb::core::schema
