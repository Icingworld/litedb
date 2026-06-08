#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/catalog/catalog_default_expression.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_id.hpp"

namespace litedb::core::catalog
{

/**
 * @brief 目录项类型
 */
enum class CatalogEntryKind : std::uint8_t
{
    Database,
    Collection,
    Column,
    Index,
    VectorIndex,
};

/**
 * @brief 规范化标识符
 * @param name 标识符
 * @return 规范化后的标识符
 */
[[nodiscard]]
std::string normalize_identifier(std::string_view name);

/**
 * @brief 目录项
 */
class CatalogEntry
{
public:
    /**
     * @brief 构造目录项
     * @param kind 目录项类型
     * @param id 目录项 ID
     * @param name 目录项名称
     */
    CatalogEntry(CatalogEntryKind kind, std::uint64_t id, std::string name);

    CatalogEntry(const CatalogEntry &) = delete;

    CatalogEntry & operator=(const CatalogEntry &) = delete;

    CatalogEntry(CatalogEntry &&) noexcept = default;

    CatalogEntry & operator=(CatalogEntry &&) noexcept = default;

    virtual ~CatalogEntry() noexcept = default;

public:
    /**
     * @brief 获取目录项类型
     * @return 目录项类型
     */
    [[nodiscard]]
    CatalogEntryKind kind() const noexcept;

    /**
     * @brief 获取目录项原始 ID
     * @return 目录项原始 ID
     */
    [[nodiscard]]
    std::uint64_t raw_id() const noexcept;

    /**
     * @brief 获取目录项名称
     * @return 目录项名称
     */
    [[nodiscard]]
    const std::string & name() const noexcept;

    /**
     * @brief 获取目录项键
     * @return 目录项键
     */
    [[nodiscard]]
    const std::string & key() const noexcept;

private:
    CatalogEntryKind kind_;     ///< 目录项类型
    std::uint64_t id_;          ///< 目录项原始 ID
    std::string name_;          ///< 目录项原始名称
    std::string key_;           ///< 目录项规范化后的键
};

/**
 * @brief 数据库目录项
 */
class DatabaseEntry final : public CatalogEntry
{
public:
    DatabaseEntry(common::DatabaseId id, std::string name);

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId id() const noexcept;

    /**
     * @brief 获取数据库包含的集合 ID 列表
     * @return 数据库包含的集合 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::CollectionId> & collection_ids() const noexcept;

    /**
     * @brief 查找集合 ID
     * @param collection_key 集合键
     * @return 集合 ID
     */
    [[nodiscard]]
    std::optional<common::CollectionId> find_collection_id(std::string_view collection_key) const;

    /**
     * @brief 添加集合
     * @param collection_key 集合键
     * @param collection_id 集合 ID
     */
    void add_collection(std::string_view collection_key, common::CollectionId collection_id);

    /**
     * @brief 删除集合
     * @param collection_key 集合键
     * @param collection_id 集合 ID
     */
    void remove_collection(std::string_view collection_key, common::CollectionId collection_id);

private:
    std::vector<common::CollectionId> collection_ids_; ///< 数据库包含的集合 ID 列表
    std::unordered_map<std::string, common::CollectionId> collections_by_key_; ///< 数据库包含的集合键到 ID 的映射
};

/**
 * @brief 集合目录项
 */
class CollectionEntry final : public CatalogEntry
{
public:
    CollectionEntry(common::CollectionId id, common::DatabaseId database_id, std::string name);

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId id() const noexcept;

    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合包含的列 ID 列表
     * @return 集合包含的列 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::ColumnId> & column_ids() const noexcept;

    /**
     * @brief 获取集合主键列 ID
     * @return 集合主键列 ID
     */
    [[nodiscard]]
    std::optional<common::ColumnId> primary_key_column_id() const noexcept;

    /**
     * @brief 查找列 ID
     * @param column_key 列键
     * @return 列 ID
     */
    [[nodiscard]]
    std::optional<common::ColumnId> find_column_id(std::string_view column_key) const;

    /**
     * @brief 添加列
     * @param column_key 列键
     * @param column_id 列 ID
     * @param primary_key 是否为主键
     */
    void add_column(std::string_view column_key, common::ColumnId column_id, bool primary_key);

private:
    common::DatabaseId database_id_;                            ///< 数据库 ID
    std::vector<common::ColumnId> column_ids_;                  ///< 集合包含的列 ID 列表
    std::unordered_map<std::string, common::ColumnId> columns_by_key_;  ///< 集合包含的列键到 ID 的映射
    std::optional<common::ColumnId> primary_key_column_id_;     ///< 集合主键列 ID
};

/**
 * @brief 列目录项
 */
class ColumnEntry final : public CatalogEntry
{
public:
    /**
     * @brief 构造列目录项
     * @param id 列 ID
     * @param collection_id 集合 ID
     * @param name 列名称
     * @param type 列类型
     * @param primary_key 是否为主键
     * @param unique 是否唯一
     * @param nullable 是否可为空
     * @param default_expression 默认值表达式
     * @param comment 注释
     */
    ColumnEntry(
        common::ColumnId id,
        common::CollectionId collection_id,
        std::string name,
        common::LogicalType type,
        bool primary_key,
        bool unique,
        bool nullable,
        std::optional<CatalogDefaultExpression> default_expression,
        std::optional<std::string> comment
    );

public:
    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取列类型
     * @return 列类型
     */
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    /**
     * @brief 是否为主键
     * @return 是否为主键
     */
    [[nodiscard]]
    bool primary_key() const noexcept;

    /**
     * @brief 是否唯一
     * @return 是否唯一
     */
    [[nodiscard]]
    bool unique() const noexcept;

    /**
     * @brief 是否可为空
     * @return 是否可为空
     */
    [[nodiscard]]
    bool nullable() const noexcept;

    /**
     * @brief 获取默认值表达式
     * @return 默认值表达式
     */
    [[nodiscard]]
    const std::optional<CatalogDefaultExpression> & default_expression() const noexcept;

    /**
     * @brief 获取注释
     * @return 注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::CollectionId collection_id_;    ///< 集合 ID
    common::LogicalType type_;              ///< 列类型
    bool primary_key_;                      ///< 是否为主键
    bool unique_;                           ///< 是否唯一
    bool nullable_;                         ///< 是否可为空
    std::optional<CatalogDefaultExpression> default_expression_;  ///< 默认值表达式
    std::optional<std::string> comment_;    ///< 注释
};

} // namespace litedb::core::catalog
