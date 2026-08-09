#include "core/common/value.hpp"

#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>

namespace litedb::core::common
{

Value::Value() = default;

Value::Value(ValueData data)
    : data_(std::move(data))
{}

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

std::string value_to_string(const Value & value)
{
    return std::visit(
        [](const auto & data) -> std::string {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, NullValue>) {
                return "NULL";
            } else if constexpr (std::is_same_v<T, bool>) {
                return data ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return data;
            } else if constexpr (std::is_same_v<T, VectorValue>) {
                std::ostringstream out;
                out << '[';
                for (std::size_t index = 0; index < data.size(); ++index) {
                    if (index != 0) {
                        out << ", ";
                    }
                    out << std::setprecision(std::numeric_limits<double>::max_digits10)
                        << data[index];
                }
                out << ']';
                return out.str();
            } else if constexpr (std::is_floating_point_v<T>) {
                std::ostringstream out;
                out << std::setprecision(std::numeric_limits<T>::max_digits10) << data;
                return out.str();
            } else {
                return std::to_string(data);
            }
        },
        value.data()
    );
}

} // namespace litedb::core::common
