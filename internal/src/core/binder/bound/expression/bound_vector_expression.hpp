#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定向量表达式
 */
class BoundVectorExpression final : public BoundExpression
{
public:
    BoundVectorExpression(
        std::vector<std::unique_ptr<BoundExpression>> elements
    );

public:
    /**
     * @brief 获取元素列表
     * @return 元素列表
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> &
    elements() const noexcept;

    /**
     * @brief 移出向量元素
     * @return 向量元素所有权
     */
    [[nodiscard]]
    std::vector<std::unique_ptr<BoundExpression>> take_elements() noexcept;

private:
    std::vector<std::unique_ptr<BoundExpression>> elements_;      // 元素列表
};

} // namespace litedb::core::binder::bound
