#include "core/index/index_store.hpp"

#include <utility>

namespace litedb::core::index
{

namespace
{

/**
 * @brief 创建索引错误
 */
IndexError make_error(IndexErrorCode code, std::string message)
{
    return IndexError {code, std::move(message)};
}

} // namespace

IndexStore::IndexStore(IndexDescriptor descriptor, std::unique_ptr<ScalarIndex> backend) noexcept
    : descriptor_(std::move(descriptor))
    , backend_(std::move(backend))
{
}

const IndexDescriptor & IndexStore::descriptor() const noexcept
{
    return descriptor_;
}

std::expected<void, IndexError> IndexStore::validate_key(const ScalarIndexKey & key) const
{
    if (!key.value().matches_type(descriptor_.key_type)) {
        return std::unexpected(make_error(
            IndexErrorCode::KeyTypeMismatch,
            "Index key type does not match indexed column type"
        ));
    }
    return {};
}

std::expected<void, IndexError> IndexStore::validate_unique(const ScalarIndexKey & key) const
{
    if (!descriptor_.unique) {
        return {};
    }

    auto existing = backend_->find_equal(key);
    if (!existing.has_value()) {
        return std::unexpected(std::move(existing.error()));
    }
    if (!existing->empty()) {
        return std::unexpected(make_error(IndexErrorCode::DuplicateKey, "Unique index key already exists"));
    }
    return {};
}

std::expected<void, IndexError> IndexStore::validate_insert(const ScalarIndexKey & key) const
{
    auto valid = validate_key(key);
    if (!valid.has_value()) {
        return std::unexpected(std::move(valid.error()));
    }
    return validate_unique(key);
}

std::expected<void, IndexError> IndexStore::insert(const ScalarIndexKey & key, common::RecordId record_id)
{
    auto valid = validate_insert(key);
    if (!valid.has_value()) {
        return std::unexpected(std::move(valid.error()));
    }
    return backend_->insert(key, record_id);
}

std::expected<void, IndexError> IndexStore::erase(const ScalarIndexKey & key, common::RecordId record_id)
{
    auto valid = validate_key(key);
    if (!valid.has_value()) {
        return std::unexpected(std::move(valid.error()));
    }
    return backend_->erase(key, record_id);
}

std::expected<std::vector<common::RecordId>, IndexError> IndexStore::find_equal(const ScalarIndexKey & key) const
{
    auto valid = validate_key(key);
    if (!valid.has_value()) {
        return std::unexpected(std::move(valid.error()));
    }
    return backend_->find_equal(key);
}

std::expected<std::vector<common::RecordId>, IndexError> IndexStore::scan_range(const IndexRange & range) const
{
    if (range.lower().has_value()) {
        auto valid = validate_key(range.lower()->key);
        if (!valid.has_value()) {
            return std::unexpected(std::move(valid.error()));
        }
    }
    if (range.upper().has_value()) {
        auto valid = validate_key(range.upper()->key);
        if (!valid.has_value()) {
            return std::unexpected(std::move(valid.error()));
        }
    }

    const auto * ordered = dynamic_cast<const OrderedScalarIndex *>(backend_.get());
    if (ordered == nullptr) {
        return std::unexpected(make_error(
            IndexErrorCode::UnsupportedRangeScan,
            "Index does not support range scans"
        ));
    }
    return ordered->scan_range(range);
}

std::size_t IndexStore::size() const noexcept
{
    return backend_->size();
}

} // namespace litedb::core::index
