#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "core/common/logical_type.hpp"

namespace litedb::core::schema
{

using NullValue = std::monostate;
using VectorValue = std::vector<double>;
using ValueData = std::variant<NullValue, bool, std::int32_t, std::int64_t, float, double, std::string, VectorValue>;

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
    bool matches_type(const common::LogicalType & type) const noexcept;

private:
    ValueData data_;     ///< 值数据
};

} // namespace litedb::core::schema
