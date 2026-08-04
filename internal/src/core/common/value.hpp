#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/common/logical_type.hpp"

namespace litedb::core::common
{

/**
 * @brief 空值类型
 */
using NullValue = std::monostate;

/**
 * @brief 向量值类型
 */
using VectorValue = std::vector<double>;

/**
 * @brief 值数据类型
 */
using ValueData = std::variant<
    NullValue,
    bool,
    std::int32_t,
    std::int64_t,
    float,
    double,
    std::string,
    VectorValue
    >;

/**
 * @brief 逻辑值
 */
class Value
{
public:
    Value();

    explicit Value(ValueData data);

public:
    /**
     * @brief 获取空值
     * @return 空值
     */
    [[nodiscard]]
    static Value null();

    /**
     * @brief 是否为空
     * @return 是否为空
     */
    [[nodiscard]]
    bool is_null() const noexcept;

    /**
     * @brief 获取值数据
     * @return 值数据
     */
    [[nodiscard]]
    const ValueData & data() const noexcept;

    /**
     * @brief 是否匹配类型
     * @param type 类型
     * @return 是否匹配类型
     */
    [[nodiscard]]
    bool matches_type(const LogicalType & type) const noexcept;

private:
    ValueData data_;     ///< 值数据
};

/**
 * @brief 将值转换为字符串
 * @param value 值
 * @return 字符串
 */
[[nodiscard]]
std::string value_to_string(const Value & value);

} // namespace litedb::core::common
