#pragma once

#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief DESCRIBE COLLECTION 语句节点
 * @details 示例：DESCRIBE COLLECTION collection_name
 */
class BoundDescribeCollectionStatement final : public BoundStatement
{
public:
    BoundDescribeCollectionStatement(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合ID
     * @return 集合ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    common::DatabaseId database_id_;            ///< 数据库ID
    common::CollectionId collection_id_;        ///< 集合ID
    std::string collection_name_;               ///< 集合名称
};

} // namespace litedb::core::binder::bound
