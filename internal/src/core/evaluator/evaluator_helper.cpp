#include "core/evaluator/evaluator_helper.hpp"

namespace litedb::core::evaluator
{

EvaluationError make_error(EvaluationErrorCode code, std::string message)
{
    return EvaluationError {
        code,
        message
    };
}

} // namespace litedb::core::evaluator