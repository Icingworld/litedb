#pragma once

#include <expected>
#include <filesystem>

#include "core/catalog/catalog_state.hpp"
#include "core/catalog/catalog_store.hpp"
#include "core/catalog/catalog_viewer.hpp"
#include "core/filesystem/filesystem.hpp"

namespace litedb::core::catalog
{

// 目录发布者
// 在线目录的唯一发布入口，负责加载与提交后的状态发布
class CatalogPublisher
{
public:
    CatalogPublisher(std::filesystem::path path, filesystem::FileSystem & filesystem);

public:
    // 打开已有目录，若不存在则初始化空目录
    [[nodiscard]]
    std::expected<void, CatalogError> open_or_initialize();

    // 发布已提交的目录快照到内存状态
    // 该接口在事务提交、应用后调用，以确保内存状态已经与磁盘状态一致
    [[nodiscard]]
    std::expected<void, CatalogError> publish_committed(const CatalogSnapshot & snapshot);

    // 获取当前发布者的只读查看器
    [[nodiscard]]
    CatalogViewer view() const noexcept;

    // 获取当前发布者对应的完整目录快照
    [[nodiscard]]
    CatalogSnapshot snapshot() const;

private:
    CatalogState state_;
    CatalogStore store_;
};

} // namespace litedb::core::catalog
