#pragma once

namespace litedb::core::binder::bound
{

class BoundCreateDatabaseStatement;
class BoundCreateCollectionStatement;
class BoundCreateIndexStatement;
class BoundCreateVectorIndexStatement;
class BoundDeleteStatement;
class BoundDescribeCollectionStatement;
class BoundDropDatabaseStatement;
class BoundDropCollectionStatement;
class BoundDropIndexStatement;
class BoundDropVectorIndexStatement;
class BoundInsertStatement;
class BoundSelectStatement;
class BoundShowDatabasesStatement;
class BoundShowCollectionsStatement;
class BoundShowIndexesStatement;
class BoundShowVectorIndexesStatement;
class BoundUpdateStatement;
class BoundUseStatement;

/**
 * @brief 绑定语句访问器
 */
class BoundStatementVisitor
{
public:
    virtual ~BoundStatementVisitor() noexcept = default;

public:
    virtual void visit(const BoundCreateDatabaseStatement & statement) = 0;
    virtual void visit(const BoundCreateCollectionStatement & statement) = 0;
    virtual void visit(const BoundCreateIndexStatement & statement) = 0;
    virtual void visit(const BoundCreateVectorIndexStatement & statement) = 0;
    virtual void visit(const BoundDeleteStatement & statement) = 0;
    virtual void visit(const BoundDescribeCollectionStatement & statement) = 0;
    virtual void visit(const BoundDropDatabaseStatement & statement) = 0;
    virtual void visit(const BoundDropCollectionStatement & statement) = 0;
    virtual void visit(const BoundDropIndexStatement & statement) = 0;
    virtual void visit(const BoundDropVectorIndexStatement & statement) = 0;
    virtual void visit(const BoundInsertStatement & statement) = 0;
    virtual void visit(const BoundSelectStatement & statement) = 0;
    virtual void visit(const BoundShowDatabasesStatement & statement) = 0;
    virtual void visit(const BoundShowCollectionsStatement & statement) = 0;
    virtual void visit(const BoundShowIndexesStatement & statement) = 0;
    virtual void visit(const BoundShowVectorIndexesStatement & statement) = 0;
    virtual void visit(const BoundUpdateStatement & statement) = 0;
    virtual void visit(const BoundUseStatement & statement) = 0;
};

} // namespace litedb::core::binder::bound
