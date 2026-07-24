#pragma once

#include <expected>
#include <filesystem>
#include <memory>

#include "core/filesystem/filesystem.hpp"
#include "core/wal/file_write_batch.hpp"

namespace litedb::core::transaction
{

/**
 * @brief 事务准备阶段使用的稀疏文件覆盖层
 *
 * 逻辑根目录下的访问映射到正式数据目录；写入仅保存在 4 KiB dirty block 中，
 * export_batch() 将最终差异转换为 redo WAL 文件操作。
 */
class TransactionFileOverlay final
{
public:
    struct State;

    TransactionFileOverlay(
        std::filesystem::path logical_root,
        std::filesystem::path base_root,
        filesystem::FileSystem & base_filesystem
    );

    TransactionFileOverlay(const TransactionFileOverlay &) = delete;
    TransactionFileOverlay & operator=(const TransactionFileOverlay &) = delete;
    ~TransactionFileOverlay();

    [[nodiscard]]
    filesystem::FileSystem & filesystem() noexcept;

    [[nodiscard]]
    std::expected<wal::FileWriteBatch, error::Error> export_batch();

private:
    std::shared_ptr<State> state_;
    filesystem::FileSystem filesystem_;
};

} // namespace litedb::core::transaction
