#pragma once

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/common/logical_type.hpp"
#include "core/function/scalar_function.hpp"

namespace litedb::core::function
{

class FunctionCatalog;

/**
 * @brief 函数目录构建器
 */
class FunctionCatalogBuilder final
{
public:
    FunctionCatalogBuilder() = default;

public:
    /**
     * @brief 注册标量函数
     * @param name 函数名称
     * @param overload 函数重载
     * @return 是否成功
     */
    [[nodiscard]]
    std::expected<void, FunctionError> register_scalar(
        std::string_view name,
        ScalarFunctionOverload overload
    );

    /**
     * @brief 构建函数目录
     * @return 函数目录
     * @details 只允许对右值调用方法，因为会转移映射表的所有权
     */
    [[nodiscard]]
    std::expected<FunctionCatalog, FunctionError> build() &&;

private:
    std::unordered_map<
        std::string,
        std::vector<std::shared_ptr<const ScalarFunctionOverload>>
    > functions_;                  // 函数名称到函数重载列表的映射
};

/**
 * @brief 函数目录
 */
class FunctionCatalog final
{
public:
    FunctionCatalog(const FunctionCatalog &) = default;

    FunctionCatalog & operator=(const FunctionCatalog &) = default;

    FunctionCatalog(FunctionCatalog &&) noexcept = default;

    FunctionCatalog & operator=(FunctionCatalog &&) noexcept = default;

public:
    /**
     * @brief 绑定标量函数
     * @param name 函数名称
     * @param argument_types 参数类型
     * @return 绑定后的函数
     */
    [[nodiscard]]
    std::expected<BoundScalarFunction, FunctionError> bind_scalar(
        std::string_view name,
        std::span<const common::LogicalType> argument_types
    ) const;

    /**
     * @brief 函数是否存在
     * @param name 函数名称
     * @return 函数是否存在
     */
    [[nodiscard]]
    bool contains(std::string_view name) const;

private:
    explicit FunctionCatalog(
        std::unordered_map<
            std::string,
            std::vector<std::shared_ptr<const ScalarFunctionOverload>>
        > functions
    );

    friend class FunctionCatalogBuilder;

    std::unordered_map<
        std::string,
        std::vector<std::shared_ptr<const ScalarFunctionOverload>>
    > functions_;                  // 函数名称到函数重载列表的映射
};

} // namespace litedb::core::function
