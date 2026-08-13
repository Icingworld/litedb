#include "core/catalog/catalog_publisher.hpp"

#include <utility>

namespace litedb::core::catalog
{

CatalogPublisher::CatalogPublisher(std::filesystem::path path, filesystem::FileSystem & filesystem)
    : store_(std::move(path), filesystem)
{}

std::expected<void, CatalogError> CatalogPublisher::open_or_initialize()
{
    auto loaded = store_.load();
    if (!loaded) [[unlikely]] {
        return std::unexpected(std::move(loaded.error()));
    }
    CatalogSnapshot snapshot;
    if (*loaded) {
        snapshot = std::move(**loaded);
    } else {
        if (auto saved = store_.save(snapshot); !saved) [[unlikely]] {
            return std::unexpected(std::move(saved.error()));
        }
    }
    return publish_committed(snapshot);
}

std::expected<void, CatalogError> CatalogPublisher::publish_committed(
    const CatalogSnapshot & snapshot
)
{
    auto rebuilt = build_catalog_state(snapshot);
    if (!rebuilt) [[unlikely]] {
        return std::unexpected(std::move(rebuilt.error()));
    }
    state_ = std::move(*rebuilt);
    return {};
}

CatalogViewer CatalogPublisher::view() const noexcept
{
    return CatalogViewer {state_};
}

CatalogSnapshot CatalogPublisher::snapshot() const
{
    return state_.snapshot();
}

} // namespace litedb::core::catalog
