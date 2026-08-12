#pragma once

#include <expected>

#include "core/catalog/catalog_request.hpp"
#include "core/catalog/catalog_state.hpp"
#include "core/catalog/catalog_viewer.hpp"

namespace litedb::core::catalog
{

// 目录编辑器
// 对离线目录状态进行编辑，不执行文件 IO
class CatalogEditor
{
public:
    CatalogEditor() = default;

    CatalogEditor(CatalogEditor &&) noexcept = default;

    CatalogEditor & operator=(CatalogEditor &&) noexcept = default;

    CatalogEditor(const CatalogEditor &) = delete;

    CatalogEditor & operator=(const CatalogEditor &) = delete;

public:
    // 从已有查看器复制出可编辑状态
    [[nodiscard]]
    static std::expected<CatalogEditor, CatalogError> from(CatalogViewer source);

    // 从快照构建可编辑状态
    [[nodiscard]]
    static std::expected<CatalogEditor, CatalogError> from(const CatalogSnapshot & source);

    // 获取当前编辑器的只读查看器
    [[nodiscard]]
    CatalogViewer view() const noexcept;

    // 获取当前编辑器对应的完整目录快照
    [[nodiscard]]
    CatalogSnapshot snapshot() const;

    // 创建数据库
    [[nodiscard]]
    std::expected<common::DatabaseId, CatalogError> create_database(
        const CreateDatabaseRequest & request
    );

    // 删除数据库
    [[nodiscard]]
    std::expected<void, CatalogError> drop_database(const DropDatabaseRequest & request);

    // 创建集合
    [[nodiscard]]
    std::expected<common::CollectionId, CatalogError> create_collection(
        const CreateCollectionRequest & request
    );

    // 删除集合
    [[nodiscard]]
    std::expected<void, CatalogError> drop_collection(const DropCollectionRequest & request);

    // 创建标量索引
    [[nodiscard]]
    std::expected<common::IndexId, CatalogError> create_index(const CreateIndexRequest & request);

    // 删除标量索引
    [[nodiscard]]
    std::expected<void, CatalogError> drop_index(const DropIndexRequest & request);

    // 创建向量索引
    [[nodiscard]]
    std::expected<common::VIndexId, CatalogError> create_vector_index(
        const CreateVectorIndexRequest & request
    );

    // 删除向量索引
    [[nodiscard]]
    std::expected<void, CatalogError> drop_vector_index(const DropVectorIndexRequest & request);

private:
    CatalogState state_;
};

} // namespace litedb::core::catalog
