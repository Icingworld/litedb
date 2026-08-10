#pragma once

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/common/logical_type.hpp"
#include "core/common/value.hpp"
#include "core/function/function_error.hpp"
#include "core/function/function_parameters.hpp"
#include "core/function/function_properties.hpp"

namespace litedb::core::function
{

// 标量函数上下文
// 用于存储标量函数执行时所需的环境和状态
// 未来可以放入事务、时间、时区等等，目前不需要，所以是空的
struct ScalarFunctionContext
{};

// 函数绑定数据
// 用于存储函数绑定时所提前准备的数据
// 例如：提前编译正则表达式，提前计算哈希值等等
class FunctionBindData
{
public:
    virtual ~FunctionBindData() noexcept = default;
};

// 函数绑定结果
// 描述一次函数调用所需要的参数类型、返回类型和绑定数据
struct ScalarBindResult
{
    std::vector<common::LogicalType> argument_types;
    common::LogicalType return_type;
    std::shared_ptr<const FunctionBindData> bind_data;
};

// 函数重载定义
// 描述一个函数可以接受哪些参数类型，返回什么类型，以及如何绑定和执行
// 在函数注册阶段，需要提供重载定义
struct ScalarFunctionOverload
{
    // 绑定函数类型
    // 该函数在 binder 阶段调用，直接收类型，返回绑定结果
    // 在绑定过程中，可能会执行参数验证、返回值推断、绑定数据提前计算等等
    using BindFn = std::expected<ScalarBindResult, FunctionError> (*)(
        std::span<const common::LogicalType> argument_types
    );

    // 执行函数类型
    // 该函数用于实际执行函数，返回结果值
    using EvalFn = std::expected<common::Value, FunctionError> (*)(
        std::span<const common::Value> arguments,
        const ScalarFunctionContext & context,
        const FunctionBindData * bind_data
    );

    FunctionParameters parameters;
    common::LogicalType return_type;
    BindFn bind {nullptr};
    EvalFn evaluate {nullptr};
    FunctionProperties properties {};
};

// 已绑定函数
// 在 binder 阶段，选定了某一个函数重载作为具体函数，并完成类型解析
class BoundScalarFunction final
{
public:
    BoundScalarFunction(
        std::string name,
        std::shared_ptr<const ScalarFunctionOverload> overload,
        std::vector<common::LogicalType> argument_types,
        common::LogicalType return_type,
        std::shared_ptr<const FunctionBindData> bind_data,
        std::size_t match_cost
    );

public:
    // 获取函数重载
    [[nodiscard]]
    const ScalarFunctionOverload & overload() const noexcept;

    // 获取函数名称
    [[nodiscard]]
    const std::string & name() const noexcept;

    // 获取参数类型
    [[nodiscard]]
    const std::vector<common::LogicalType> & argument_types() const noexcept;

    // 获取返回类型
    [[nodiscard]]
    const common::LogicalType & return_type() const noexcept;

    // 获取函数属性
    [[nodiscard]]
    const FunctionProperties & properties() const noexcept;

    // 获取匹配成本
    [[nodiscard]]
    std::size_t match_cost() const noexcept;

    // 执行函数
    [[nodiscard]]
    std::expected<common::Value, FunctionError>
    evaluate(std::span<const common::Value> arguments, const ScalarFunctionContext & context) const;

private:
    std::string name_;
    std::shared_ptr<const ScalarFunctionOverload> overload_;
    std::vector<common::LogicalType> argument_types_;
    common::LogicalType return_type_;
    std::shared_ptr<const FunctionBindData> bind_data_;
    std::size_t match_cost_;
};

} // namespace litedb::core::function
