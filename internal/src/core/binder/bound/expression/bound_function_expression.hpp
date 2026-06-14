#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/function/function_signature.hpp"
#include "core/function/scalar_function.hpp"

namespace litedb::core::binder::bound
{

class BoundFunctionExpression final : public BoundExpression
{
public:
    BoundFunctionExpression(
        std::string name,
        std::shared_ptr<const function::ScalarFunction> function,
        function::FunctionSignature signature,
        std::vector<std::unique_ptr<BoundExpression>> arguments,
        common::LogicalType type,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    const std::string & name() const noexcept;

    [[nodiscard]]
    const function::ScalarFunction & function() const noexcept;

    [[nodiscard]]
    const function::FunctionSignature & signature() const noexcept;

    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & arguments() const noexcept;

private:
    std::string name_;
    std::shared_ptr<const function::ScalarFunction> function_;
    function::FunctionSignature signature_;
    std::vector<std::unique_ptr<BoundExpression>> arguments_;
};

} // namespace litedb::core::binder::bound
