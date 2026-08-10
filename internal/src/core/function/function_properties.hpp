#pragma once

#include <cstdint>

namespace litedb::core::function
{

// 函数稳定性
enum class FunctionVolatility : uint8_t
{
    Immutable = 0,                           // 不可变，相同输入永远得到相同结果，不依赖时间、随机数或外部状态
    Stable = 1,                              // 稳定，在同一条 SQL 语句中，或同一个事务中，相同输入永远得到相同结果
    Volatile = 2,                            // 易变，每次调用都可能得到不同结果
};

// 函数空值处理
enum class FunctionNullHandling : uint8_t
{
    PropagateNull = 0,                      // 只要有一个参数为 NULL，结果就为 NULL
    CalledOnNull = 1,                       // 即使参数有 NULL，也会调用函数
};

// 函数语义标签
// 用于描述该函数除了输入和输出之外，在数据库中还代表了什么特殊含义
// 例如：
// - VectorL2Distance 表示向量之间的 L2 距离，因此优化器可以考虑使用 L2 向量索引
enum class FunctionSemanticTag : uint8_t
{
    None = 0,                               // 无特殊含义
    VectorL2Distance = 1,                   // 向量之间的 L2 距离
    VectorCosineDistance = 2,               // 向量之间的余弦距离
    VectorInnerProduct = 3,                 // 向量之间的内积
};

// 函数属性
struct FunctionProperties
{
    FunctionVolatility volatility {FunctionVolatility::Immutable};
    FunctionNullHandling null_handling {FunctionNullHandling::PropagateNull};
    bool has_side_effects {false}; // 函数是否对外部产生影响
    FunctionSemanticTag semantic_tag {FunctionSemanticTag::None};
};

} // namespace litedb::core::function
