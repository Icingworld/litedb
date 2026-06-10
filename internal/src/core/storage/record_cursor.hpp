#pragma once

#include <optional>

#include "core/schema/record.hpp"

namespace litedb::core::storage
{

/**
 * @brief 记录游标
 */
class RecordCursor
{
public:
    virtual ~RecordCursor() noexcept = default;

public:
    /**
     * @brief 获取下一条记录
     * @return 下一条记录，不存在时返回空
     */
    [[nodiscard]]
    virtual std::optional<schema::Record> next() = 0;
};

} // namespace litedb::core::storage
