#include "core/binder/detail/literal_value_parser.hpp"

#include <cstdint>
#include <exception>
#include <limits>

namespace litedb::core::binder::detail
{

std::optional<common::Value>
parse_literal_value(common::LogicalTypeId type_id, const std::string & text)
{
    try {
        std::size_t parsed = 0;
        switch (type_id) {
        case common::LogicalTypeId::Boolean:
            if (text == "true" || text == "TRUE") {
                return common::Value {common::ValueData {true}};
            }
            if (text == "false" || text == "FALSE") {
                return common::Value {common::ValueData {false}};
            }
            break;
        case common::LogicalTypeId::Integer: {
            const auto value = std::stoll(text, &parsed);
            if (parsed == text.size() && value >= std::numeric_limits<std::int32_t>::min() &&
                value <= std::numeric_limits<std::int32_t>::max()) {
                return common::Value {common::ValueData {static_cast<std::int32_t>(value)}};
            }
            break;
        }
        case common::LogicalTypeId::BigInt: {
            const auto value = std::stoll(text, &parsed);
            if (parsed == text.size()) {
                return common::Value {common::ValueData {static_cast<std::int64_t>(value)}};
            }
            break;
        }
        case common::LogicalTypeId::Float: {
            const auto value = std::stof(text, &parsed);
            if (parsed == text.size()) {
                return common::Value {common::ValueData {value}};
            }
            break;
        }
        case common::LogicalTypeId::Double: {
            const auto value = std::stod(text, &parsed);
            if (parsed == text.size()) {
                return common::Value {common::ValueData {value}};
            }
            break;
        }
        case common::LogicalTypeId::Varchar:
            return common::Value {common::ValueData {text}};
        case common::LogicalTypeId::Null:
            return common::Value::null();
        case common::LogicalTypeId::Vector:
            break;
        }
    } catch (const std::exception &) {
    }
    return std::nullopt;
}

} // namespace litedb::core::binder::detail
