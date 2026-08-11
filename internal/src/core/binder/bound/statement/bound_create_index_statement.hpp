#pragma once

#include <optional>
#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/index_entry.hpp"

namespace litedb::core::binder::bound
{

// 绑定 CREATE INDEX 语句
class BoundCreateIndexStatement final : public BoundStatement
{
public:
    BoundCreateIndexStatement(
        common::ColumnId column_id,
        std::optional<std::string> index_name,
        meta::entry::IndexKind index_kind,
        bool unique
    );

public:
    // 获取索引名称
    [[nodiscard]]
    std::optional<const std::string &> index_name() const noexcept;

    // 获取索引名称所有权
    // 调用后 index_name() 返回 nullopt；再次调用返回 nullopt
    [[nodiscard]]
    std::optional<std::string> take_index_name() noexcept;

    // 获取列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 获取索引类型
    [[nodiscard]]
    meta::entry::IndexKind index_kind() const noexcept;

    // 是否唯一
    [[nodiscard]]
    bool unique() const noexcept;

private:
    common::ColumnId column_id_;
    // index_name_ 为 nullopt 时表示用户传入了重复索引名但是用了 IF NOT EXISTS
    std::optional<std::string> index_name_;
    meta::entry::IndexKind index_kind_;
    bool unique_;
};

} // namespace litedb::core::binder::bound
