#pragma once

#include "core/evaluator/evaluation_error.hpp"

namespace litedb::core::evaluator
{

/**
 * @brief 创建评估错误
 * @param code 错误码
 * @param message 错误消息
 * @return 评估错误
 */
[[nodiscard]]
EvaluationError make_error(
    EvaluationErrorCode code,
    std::string message
);

} // namespace litedb::core::evaluator
