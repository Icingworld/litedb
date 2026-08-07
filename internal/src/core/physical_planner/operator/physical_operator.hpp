#pragma once

#include <cstdint>

namespace litedb::core::physical_planner::op
{

/**
 * @brief 物理算子类型
 */
enum class PhysicalOperatorKind : std::uint8_t
{
    SeqScan,            // 顺序扫描
    IndexScan,          // 索引扫描
    VectorSearch,       // 向量检索
    Filter,             // 过滤
    Projection,         // 投影
    Sort,               // 排序
    Limit,              // 限制
};

/**
 * @brief 物理算子
 */
class PhysicalOperator
{
public:
    PhysicalOperator(const PhysicalOperator &) = delete;

    PhysicalOperator & operator=(const PhysicalOperator &) = delete;

    PhysicalOperator(PhysicalOperator &&) noexcept = default;

    PhysicalOperator & operator=(PhysicalOperator &&) noexcept = default;

    virtual ~PhysicalOperator() noexcept = default;

protected:
    explicit PhysicalOperator(PhysicalOperatorKind kind) noexcept;

public:
    /**
     * @brief 获取算子类型
     * @return 算子类型
     */
    [[nodiscard]]
    PhysicalOperatorKind kind() const noexcept;

private:
    PhysicalOperatorKind kind_;                 // 算子类型
};

} // namespace litedb::core::physical_planner::op
