#pragma once

#include "core/evaluator/evaluation_error.hpp"

namespace litedb::core::evaluator
{

// 创建评估错误
[[nodiscard]]
EvaluationError make_error(EvaluationErrorCode code, std::string message);

} // namespace litedb::core::evaluator
