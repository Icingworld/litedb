#pragma once

#include <cstdint>

namespace litedb::core::physical_planner::op
{

// 物理算子类型
enum class PhysicalOperatorKind : std::uint8_t
{
    SeqScan,
    IndexScan,
    VectorSearch,
    Filter,
    Projection,
    Sort,
    Limit,
};

// 物理算子
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
    // 获取算子类型
    [[nodiscard]]
    PhysicalOperatorKind kind() const noexcept;

private:
    PhysicalOperatorKind kind_;
};

} // namespace litedb::core::physical_planner::op
