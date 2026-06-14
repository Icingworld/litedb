#include "core/vindex/vector_distance.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace litedb::core::vindex
{

namespace
{

[[nodiscard]]
VectorIndexError make_error(VectorIndexErrorCode code, std::string message)
{
    return VectorIndexError {code, std::move(message)};
}

/**
 * @brief 验证两个向量是否具有相同的维度
 * @param left 左向量
 * @param right 右向量
 * @return 如果两个向量具有相同的维度，则返回空结果，否则返回错误
 */
[[nodiscard]]
std::expected<void, VectorIndexError> validate_same_dimension(
    const schema::VectorValue & left,
    const schema::VectorValue & right
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
double l2_distance(const schema::VectorValue & left, const schema::VectorValue & right)
{
    double sum = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const double diff = left[index] - right[index];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

/**
 * @brief 计算两个向量之间的负内积
 * @param left 左向量
 * @param right 右向量
 * @return 两个向量之间的负内积
 */
[[nodiscard]]
double negative_inner_product(const schema::VectorValue & left, const schema::VectorValue & right)
{
    double result = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        result += left[index] * right[index];
    }
    return -result;
}

/**
 * @brief 计算两个向量之间的余弦距离
 * @param left 左向量
 * @param right 右向量
 * @return 两个向量之间的余弦距离
 */
[[nodiscard]]
double cosine_distance(const schema::VectorValue & left, const schema::VectorValue & right)
{
    double dot = 0.0;
    double left_norm = 0.0;
    double right_norm = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        dot += left[index] * right[index];
        left_norm += left[index] * left[index];
        right_norm += right[index] * right[index];
    }
    if (left_norm == 0.0 || right_norm == 0.0) {
        return 1.0;
    }
    return 1.0 - (dot / (std::sqrt(left_norm) * std::sqrt(right_norm)));
}

} // namespace

std::expected<double, VectorIndexError> vector_distance(
    const schema::VectorValue & left,
    const schema::VectorValue & right,
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
