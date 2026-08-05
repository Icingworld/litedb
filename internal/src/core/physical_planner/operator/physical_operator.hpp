#pragma once

#include <cstdint>

namespace litedb::core::physical_planner::op
{

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

class PhysicalOperator
{
public:
    PhysicalOperator(const PhysicalOperator &) = delete;
    PhysicalOperator & operator=(const PhysicalOperator &) = delete;

    PhysicalOperator(PhysicalOperator &&) noexcept = default;
    PhysicalOperator & operator=(PhysicalOperator &&) noexcept = default;

    virtual ~PhysicalOperator() noexcept = default;

protected:
    explicit PhysicalOperator(PhysicalOperatorKind kind) noexcept
        : kind_(kind)
    {
    }

public:
    [[nodiscard]] PhysicalOperatorKind kind() const noexcept
    {
        return kind_;
    }

private:
    PhysicalOperatorKind kind_;
};

} // namespace litedb::core::physical_planner::op
