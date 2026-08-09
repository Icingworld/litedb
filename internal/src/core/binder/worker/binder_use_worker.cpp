#include "core/binder/worker/binder_use_worker.hpp"

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/statement/bound_use_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderUseWorker::BinderUseWorker(const BinderContext & context) noexcept
    : context_(context)
{}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderUseWorker::bind_use(
    const UseStatement & statement
)
{
    // 查找数据库
    const auto * database = context_.meta().find_database(statement.database_name());
    if (database == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            "Database not found: " + statement.database_name()
        ));
    }

    return std::make_unique<BoundUseStatement>(database->id());
}

} // namespace litedb::core::binder
