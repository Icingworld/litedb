#pragma once

#include <string>
#include <vector>

#include "core/function/function_signature.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数
 */
class Function
{
public:
    Function(const Function &) = delete;
    
    Function & operator=(const Function &) = delete;
    
    Function(Function &&) noexcept = default;
    
    Function & operator=(Function &&) noexcept = default;

    virtual ~Function() noexcept = default;

protected:
    Function(std::string name, FunctionKind kind);

public:
    /**
     * @brief 获取函数名称
     * @return 函数名称
     */
    [[nodiscard]]
    const std::string & name() const noexcept;

    /**
     * @brief 获取函数类型
     * @return 函数类型
     */
    [[nodiscard]]
    FunctionKind kind() const noexcept;

    /**
     * @brief 获取函数签名
     * @return 函数签名
     */
    [[nodiscard]]
    virtual const std::vector<FunctionSignature> & signatures() const noexcept = 0;

private:
    std::string name_;          ///< 函数名称
    FunctionKind kind_;         ///< 函数类型
};

} // namespace litedb::core::function
