#pragma once

namespace litedb::core::binder::bound
{

class BoundBetweenExpression;
class BoundBinaryExpression;
class BoundCastExpression;
class BoundColumnReferenceExpression;
class BoundFunctionCallExpression;
class BoundInExpression;
class BoundLikeExpression;
class BoundLiteralExpression;
class BoundNullExpression;
class BoundUnaryExpression;
class BoundVectorExpression;
class BoundWildcardExpression;

/**
 * @brief 绑定表达式访问器
 */
class BoundExpressionVisitor
{
public:
    virtual ~BoundExpressionVisitor() noexcept = default;
    
public:
    virtual void visit(const BoundBetweenExpression & expression) = 0;
    virtual void visit(const BoundBinaryExpression & expression) = 0;
    virtual void visit(const BoundCastExpression & expression) = 0;
    virtual void visit(const BoundColumnReferenceExpression & expression) = 0;
    virtual void visit(const BoundFunctionCallExpression & expression) = 0;
    virtual void visit(const BoundInExpression & expression) = 0;
    virtual void visit(const BoundLikeExpression & expression) = 0;
    virtual void visit(const BoundLiteralExpression & expression) = 0;
    virtual void visit(const BoundNullExpression & expression) = 0;
    virtual void visit(const BoundUnaryExpression & expression) = 0;
    virtual void visit(const BoundVectorExpression & expression) = 0;
    virtual void visit(const BoundWildcardExpression & expression) = 0;
};

} // namespace litedb::core::binder::bound
