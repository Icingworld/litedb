#pragma once

namespace litedb::core::parser::ast
{

class AlterStatement;
class CreateCollectionStatement;
class CreateDatabaseStatement;
class CreateIndexStatement;
class CreateVectorIndexStatement;
class DeleteStatement;
class DescribeStatement;
class DropCollectionStatement;
class DropDatabaseStatement;
class DropIndexStatement;
class DropVectorIndexStatement;
class InsertStatement;
class SelectStatement;
class ShowStatement;
class UpdateStatement;
class UseStatement;
class BetweenExpression;
class BinaryExpression;
class ColumnReferenceExpression;
class FunctionCallExpression;
class IdentifierExpression;
class InExpression;
class LikeExpression;
class LiteralExpression;
class UnaryExpression;
class VectorExpression;
class WildcardExpression;

/**
 * @brief 抽象语法树节点访问器
 */
class AstNodeVisitor
{
public:
    virtual ~AstNodeVisitor() noexcept = default;

public:
    virtual void visit(const AlterStatement & node) = 0;
    virtual void visit(const CreateCollectionStatement & node) = 0;
    virtual void visit(const CreateDatabaseStatement & node) = 0;
    virtual void visit(const CreateIndexStatement & node) = 0;
    virtual void visit(const CreateVectorIndexStatement & node) = 0;
    virtual void visit(const DeleteStatement & node) = 0;
    virtual void visit(const DescribeStatement & node) = 0;
    virtual void visit(const DropCollectionStatement & node) = 0;
    virtual void visit(const DropDatabaseStatement & node) = 0;
    virtual void visit(const DropIndexStatement & node) = 0;
    virtual void visit(const DropVectorIndexStatement & node) = 0;
    virtual void visit(const InsertStatement & node) = 0;
    virtual void visit(const SelectStatement & node) = 0;
    virtual void visit(const ShowStatement & node) = 0;
    virtual void visit(const UpdateStatement & node) = 0;
    virtual void visit(const UseStatement & node) = 0;

    virtual void visit(const BetweenExpression & node) = 0;
    virtual void visit(const BinaryExpression & node) = 0;
    virtual void visit(const ColumnReferenceExpression & node) = 0;
    virtual void visit(const FunctionCallExpression & node) = 0;
    virtual void visit(const IdentifierExpression & node) = 0;
    virtual void visit(const InExpression & node) = 0;
    virtual void visit(const LikeExpression & node) = 0;
    virtual void visit(const LiteralExpression & node) = 0;
    virtual void visit(const UnaryExpression & node) = 0;
    virtual void visit(const VectorExpression & node) = 0;
    virtual void visit(const WildcardExpression & node) = 0;
};

} // namespace litedb::core::parser::ast
