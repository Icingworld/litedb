#include "core/binder/bound/debug/debug_helper.hpp"

namespace litedb::core::binder::bound
{

std::string_view logical_type_name(common::LogicalTypeId id) noexcept
{
    switch (id) {
    case common::LogicalTypeId::Null:
        return "NULL";
    case common::LogicalTypeId::Boolean:
        return "BOOLEAN";
    case common::LogicalTypeId::Integer:
        return "INTEGER";
    case common::LogicalTypeId::BigInt:
        return "BIGINT";
    case common::LogicalTypeId::Float:
        return "FLOAT";
    case common::LogicalTypeId::Double:
        return "DOUBLE";
    case common::LogicalTypeId::Varchar:
        return "VARCHAR";
    case common::LogicalTypeId::Vector:
        return "VECTOR";
    }

    return "UNKNOWN";
}

std::string logical_type_text(const common::LogicalType & type)
{
    std::string text {logical_type_name(type.id)};
    if (type.parameter.has_value()) {
        text += '(';
        text += std::to_string(*type.parameter);
        text += ')';
    }
    return text;
}

std::string_view unary_operator_name(common::UnaryOperator op) noexcept
{
    switch (op) {
    case common::UnaryOperator::Negate:
        return "Negate";
    case common::UnaryOperator::Not:
        return "Not";
    }

    return "Unknown";
}

std::string_view binary_operator_name(common::BinaryOperator op) noexcept
{
    switch (op) {
    case common::BinaryOperator::Add:
        return "Add";
    case common::BinaryOperator::Subtract:
        return "Subtract";
    case common::BinaryOperator::Multiply:
        return "Multiply";
    case common::BinaryOperator::Divide:
        return "Divide";
    case common::BinaryOperator::Modulus:
        return "Modulus";
    case common::BinaryOperator::Power:
        return "Power";
    }

    return "Unknown";
}

std::string_view index_kind_name(meta::entry::IndexKind kind) noexcept
{
    switch (kind) {
    case meta::entry::IndexKind::BTree:
        return "BTree";
    }

    return "Unknown";
}

std::string_view vector_index_kind_name(meta::entry::VectorIndexKind kind) noexcept
{
    switch (kind) {
    case meta::entry::VectorIndexKind::Hnsw:
        return "Hnsw";
    }

    return "Unknown";
}

std::string_view vector_distance_metric_name(meta::entry::VectorDistanceMetric metric) noexcept
{
    switch (metric) {
    case meta::entry::VectorDistanceMetric::L2:
        return "L2";
    case meta::entry::VectorDistanceMetric::InnerProduct:
        return "InnerProduct";
    case meta::entry::VectorDistanceMetric::Cosine:
        return "Cosine";
    }

    return "Unknown";
}

} // namespace litedb::core::binder::bound
