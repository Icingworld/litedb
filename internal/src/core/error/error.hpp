#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace litedb::core::error
{

/**
 * @brief 错误所属模块
 */
enum class ErrorCategory : std::uint8_t
{
    Unknown = 0,                ///< 未知
};

/**
 * @brief 通用错误类型模板
 * @tparam E 错误类型
 * @details 当具体的错误类型 E 被定义时，必须特化 ErrorTraits 模板类，并提供 category 静态成员变量
 * 例如:
 * template <>
 * struct ErrorTraits<MyError>
 * {
 *     static constexpr ErrorCategory category = ErrorCategory::MyError;
 * };
 */
template <typename E>
struct ErrorTraits;

/**
 * @brief 错误类型概念
 * @tparam E 错误类型
 * @details 错误类型 E 必须满足以下条件：
 * 1. E 的底层类型为 std::uint8_t
 * 2. ErrorTraits<E> 必须拥有 const ErrorCategory 类型的 category 静态成员变量
 */
template <typename E>
concept ErrorType =
    std::is_enum_v<E> &&
    std::same_as<std::underlying_type_t<E>, std::uint8_t> &&
    requires {
        { ErrorTraits<E>::category } -> std::same_as<const ErrorCategory &>;
    };

/**
 * @brief 错误
 */
class Error
{
public:
    template <ErrorType E>
    explicit Error(E error_code, std::string_view message)
        : category_(ErrorTraits<E>::category)
        , code_(std::to_underlying(error_code))
        , message_(message)
    {
    }

public:
    /**
     * @brief 获取错误所属模块
     * @return 错误所属模块
     */
    [[nodiscard]]
    ErrorCategory category() const noexcept
    {
        return category_;
    }

    /**
     * @brief 获取错误码
     * @return 错误码
     */
    [[nodiscard]]
    std::uint8_t code() const noexcept
    {
        return code_;
    }

    /**
     * @brief 获取错误信息
     * @return 错误信息
     */
    [[nodiscard]]
    const std::string & message() const noexcept
    {
        return message_;
    }

    /**
     * @brief 编码错误码
     * @return 编码后的错误码
     */
    [[nodiscard]]
    std::uint16_t encode_code() const noexcept
    {
        return (static_cast<std::uint16_t>(category_) << 8) | code_;
    }

private:
    ErrorCategory category_;    ///< 错误所属模块
    std::uint8_t code_;         ///< 错误码
    std::string message_;       ///< 错误信息
};

} // namespace litedb::core::error
