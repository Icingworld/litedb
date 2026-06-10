#include "core/schema/database.hpp"

#include <utility>

namespace litedb::core::schema
{

DatabaseSchema::DatabaseSchema(common::DatabaseId database_id, std::string database_name)
    : database_id_(database_id)
    , database_name_(std::move(database_name))
{
}

common::DatabaseId DatabaseSchema::database_id() const noexcept
{
    return database_id_;
}

const std::string & DatabaseSchema::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::schema
