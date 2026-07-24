#include "core/vindex/vector_distance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, message};
}

/**
 * @brief 验证两个向量是否具有相同的维度
 * @param left 左向量
 * @param right 右向量
 * @return 如果两个向量具有相同的维度，则返回空结果，否则返回错误
 */
[[nodiscard]]
std::expected<void, VectorIndexError> validate_same_dimension(
    const common::VectorValue & left,
    const common::VectorValue & right
)
{
    if (left.empty() || right.empty()) {
        return std::unexpected(make_error(VectorIndexErrorCode::EmptyQuery, "Vector must not be empty"));
    }
    if (left.size() != right.size()) {
        return std::unexpected(make_error(VectorIndexErrorCode::InvalidDimension, "Vector dimensions must match"));
    }
    return {};
}

/**
 * @brief 计算两个向量之间的 L2 距离
 * @param left 左向量
 * @param right 右向量
 * @return 两个向量之间的 L2 距离
 */
[[nodiscard]]
std::expected<double, VectorIndexError> checked_distance(long double value)
{
    if (!std::isfinite(value) || value > static_cast<long double>(std::numeric_limits<double>::max()) ||
        value < -static_cast<long double>(std::numeric_limits<double>::max())) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::NumericOverflow,
            "Vector distance is outside the finite double range"
        ));
    }
    const auto result = static_cast<double>(value);
    if (!std::isfinite(result)) {
        return std::unexpected(make_error(
            VectorIndexErrorCode::NumericOverflow,
            "Vector distance is outside the finite double range"
        ));
    }
    return result;
}

[[nodiscard]]
std::expected<double, VectorIndexError> l2_distance(
    const common::VectorValue & left,
    const common::VectorValue & right
)
{
    long double scale = 0.0L;
    long double sum = 1.0L;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto difference = std::fabs(
            static_cast<long double>(left[index]) - static_cast<long double>(right[index])
        );
        if (difference == 0.0L) {
            continue;
        }
        if (scale < difference) {
            const auto ratio = scale / difference;
            sum = 1.0L + sum * ratio * ratio;
            scale = difference;
        } else {
            const auto ratio = difference / scale;
            sum += ratio * ratio;
        }
    }
    return checked_distance(scale == 0.0L ? 0.0L : scale * std::sqrt(sum));
}

/**
 * @brief 计算两个向量之间的负内积
 * @param left 左向量
 * @param right 右向量
 * @return 两个向量之间的负内积
 */
[[nodiscard]]
std::expected<double, VectorIndexError> negative_inner_product(
    const common::VectorValue & left,
    const common::VectorValue & right
)
{
    long double result = 0.0L;
    for (std::size_t index = 0; index < left.size(); ++index) {
        result += static_cast<long double>(left[index]) * static_cast<long double>(right[index]);
    }
    return checked_distance(-result);
}

/**
 * @brief 计算两个向量之间的余弦距离
 * @param left 左向量
 * @param right 右向量
 * @return 两个向量之间的余弦距离
 */
[[nodiscard]]
std::expected<double, VectorIndexError> cosine_distance(
    const common::VectorValue & left,
    const common::VectorValue & right
)
{
    long double dot = 0.0L;
    long double left_norm = 0.0L;
    long double right_norm = 0.0L;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto left_value = static_cast<long double>(left[index]);
        const auto right_value = static_cast<long double>(right[index]);
        dot += left_value * right_value;
        left_norm += left_value * left_value;
        right_norm += right_value * right_value;
    }
    if (left_norm == 0.0L || right_norm == 0.0L) {
        return 1.0;
    }
    const auto similarity = std::clamp(
        dot / (std::sqrt(left_norm) * std::sqrt(right_norm)),
        -1.0L,
        1.0L
    );
    return checked_distance(1.0L - similarity);
}

} // namespace

std::expected<double, VectorIndexError> vector_distance(
    const common::VectorValue & left,
    const common::VectorValue & right,
    VectorDistanceMetric metric
)
{
    auto validation = validate_same_dimension(left, right);
    if (!validation.has_value()) {
        return std::unexpected(std::move(validation.error()));
    }

    switch (metric) {
    case VectorDistanceMetric::L2:
        return l2_distance(left, right);
    case VectorDistanceMetric::InnerProduct:
        return negative_inner_product(left, right);
    case VectorDistanceMetric::Cosine:
        return cosine_distance(left, right);
    }

    return std::unexpected(make_error(VectorIndexErrorCode::UnsupportedMetric, "Unsupported vector distance metric"));
}

} // namespace litedb::core::vindex
