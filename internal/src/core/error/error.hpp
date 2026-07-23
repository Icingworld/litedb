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
    FileSystem = 1,             ///< 文件系统
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

class Error;

/**
 * @brief 错误上下文类型概念
 * @tparam C 错误上下文类型
 * @details 错误上下文类型 C 必须满足以下条件：
 * 1. C 必须是可移动构造的对象类型
 * 2. C 不能是 Error 本身
 */
template <typename C>
concept ErrorContextType =
    std::is_object_v<std::remove_cvref_t<C>> &&
    std::move_constructible<std::remove_cvref_t<C>> &&
    (!std::same_as<std::remove_cvref_t<C>, Error>);

namespace detail
{

template <typename C>
inline constexpr unsigned char ErrorContextTypeToken = 0;

template <typename C>
[[nodiscard]]
const void * error_context_type_token() noexcept
{
    return &ErrorContextTypeToken<C>;
}

/**
 * @brief 错误上下文类型擦除容器
 */
class ErasedErrorContext
{
public:
    ErasedErrorContext() noexcept = default;

    template <ErrorContextType C>
    explicit ErasedErrorContext(C && context)
    {
        using Context = std::remove_cvref_t<C>;
        value_ = new Context(std::forward<C>(context));
        type_token_ = error_context_type_token<Context>();
        destroy_ = [](void * value) noexcept {
            delete static_cast<Context *>(value);
        };
    }

    ErasedErrorContext(const ErasedErrorContext &) = delete;

    ErasedErrorContext & operator=(const ErasedErrorContext &) = delete;

    ErasedErrorContext(ErasedErrorContext && other) noexcept
        : value_(std::exchange(other.value_, nullptr))
        , type_token_(std::exchange(other.type_token_, nullptr))
        , destroy_(std::exchange(other.destroy_, nullptr))
    {
    }

    ErasedErrorContext & operator=(ErasedErrorContext && other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        reset();
        value_ = std::exchange(other.value_, nullptr);
        type_token_ = std::exchange(other.type_token_, nullptr);
        destroy_ = std::exchange(other.destroy_, nullptr);
        return *this;
    }

    ~ErasedErrorContext()
    {
        reset();
    }

public:
    /**
     * @brief 获取错误上下文
     * @tparam C 错误上下文类型
     * @return 错误上下文，如果没有上下文，则返回 nullptr
     */
    template <typename C>
    [[nodiscard]]
    const std::remove_cvref_t<C> * get() const noexcept
    {
        using Context = std::remove_cvref_t<C>;
        if (value_ == nullptr || type_token_ != error_context_type_token<Context>()) {
            return nullptr;
        }
        return static_cast<const Context *>(value_);
    }

private:
    /**
     * @brief 重置错误上下文
     */
    void reset() noexcept
    {
        if (value_ != nullptr) {
            destroy_(value_);
        }
        value_ = nullptr;
        type_token_ = nullptr;
        destroy_ = nullptr;
    }

private:
    void * value_ {nullptr};                        ///< 错误上下文对象指针
    const void * type_token_ {nullptr};             ///< 错误上下文类型标识
    void (*destroy_)(void *) noexcept {nullptr};    ///< 错误上下文对象销毁函数
};

} // namespace detail

/**
 * @brief 错误
 * @details 错误对象可携带模块上下文和下层 cause，不可拷贝但可以移动
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

    template <ErrorType E, ErrorContextType C>
    explicit Error(E error_code, std::string_view message, C && context)
        : Error(error_code, message)
    {
        context_ = detail::ErasedErrorContext {std::forward<C>(context)};
    }

    Error(const Error &) = delete;

    Error & operator=(const Error &) = delete;

    Error(Error &&) noexcept = default;

    Error & operator=(Error &&) noexcept = default;

    ~Error() = default;

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
     * @brief 判断是否为指定的模块错误码
     * @tparam E 模块错误码枚举
     * @param error_code 待匹配的错误码
     */
    template <ErrorType E>
    [[nodiscard]]
    bool is(E error_code) const noexcept
    {
        return category_ == ErrorTraits<E>::category &&
               code_ == std::to_underlying(error_code);
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
     * @brief 获取错误上下文
     * @return 错误上下文，如果没有上下文，则返回 nullptr
     */
    template <ErrorContextType C>
    [[nodiscard]]
    const std::remove_cvref_t<C> * context() const noexcept
    {
        return context_.get<C>();
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
    ErrorCategory category_;                    ///< 错误所属模块
    std::uint8_t code_;                         ///< 错误码
    std::string message_;                       ///< 错误信息
    detail::ErasedErrorContext context_;        ///< 模块上下文
};

} // namespace litedb::core::error
