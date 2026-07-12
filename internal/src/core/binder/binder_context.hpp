#pragma once

#include "core/binder/session_context.hpp"
#include "core/meta/meta_engine.hpp"

namespace litedb::core::binder
{

/**
 * @brief 绑定上下文
 */
class BinderContext
{
public:
    BinderContext(const meta::MetaEngine & meta, const SessionContext & session) noexcept;

public:
    /**
     * @brief 获取数据库读取器
     * @return 数据库读取器
     */
    [[nodiscard]]
    const meta::MetaEngine & meta() const noexcept;

    /**
     * @brief 获取会话上下文
     * @return 会话上下文
     */
    [[nodiscard]]
    const SessionContext & session() const noexcept;

private:
    const meta::MetaEngine & meta_;       ///< 元数据引擎
    const SessionContext & session_;            ///< 会话上下文
};

} // namespace litedb::core::binder
