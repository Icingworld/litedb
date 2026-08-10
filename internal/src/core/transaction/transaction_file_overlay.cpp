#include "core/transaction/transaction_file_overlay.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/filesystem/backend/file_handle_backend.hpp"
#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::transaction
{

namespace
{

constexpr std::uint64_t BlockSize = 4096;
using Block = std::array<std::byte, BlockSize>;

error::Error overlay_error(
    filesystem::FileSystemErrorCode code,
    std::string message,
    std::string operation,
    const std::filesystem::path & path = {},
    const std::filesystem::path & related = {}
)
{
    return error::Error {code, message, filesystem::FileSystemErrorContext {
        .operation = std::move(operation),
        .path = path,
        .related_path = related,
    }};
}

bool is_within_root(const std::filesystem::path & relative)
{
    if (relative.empty()) return true;
    const auto first = *relative.begin();
    return first != "..";
}

std::optional<wal::FileTarget> target_for_relative(const std::filesystem::path & relative)
{
    const auto generic = relative.generic_string();
    if (generic == "meta.lmeta") {
        return wal::FileTarget {.kind = wal::FileKind::MetaStore, .object_id = 0};
    }
    auto parse_id = [&](std::string_view prefix, std::string_view suffix) -> std::optional<std::uint64_t> {
        if (!generic.starts_with(prefix) || !generic.ends_with(suffix)) return std::nullopt;
        const auto first = prefix.size();
        const auto count = generic.size() - prefix.size() - suffix.size();
        const auto text = generic.substr(first, count);
        if (text.empty()) return std::nullopt;
        std::uint64_t value {0};
        for (const char character : text) {
            if (character < '0' || character > '9') return std::nullopt;
            const auto digit = static_cast<std::uint64_t>(character - '0');
            if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return std::nullopt;
            value = value * 10 + digit;
        }
        return value == 0 ? std::nullopt : std::optional {value};
    };
    if (auto id = parse_id("collections/", ".store")) {
        return wal::FileTarget {.kind = wal::FileKind::CollectionStore, .object_id = *id};
    }
    if (auto id = parse_id("indexes/", ".bti")) {
        return wal::FileTarget {.kind = wal::FileKind::ScalarIndex, .object_id = *id};
    }
    if (auto id = parse_id("vindexes/vindex_", ".lhnsw")) {
        return wal::FileTarget {.kind = wal::FileKind::VectorIndex, .object_id = *id};
    }
    return std::nullopt;
}

} // namespace

struct TransactionFileOverlay::State
{
    struct Node
    {
        std::filesystem::path logical_path;
        std::filesystem::path base_path;
        bool inspected {false};
        bool initial_exists {false};
        bool exists {false};
        std::uint64_t initial_size {0};
        std::uint64_t size {0};
        std::map<std::uint64_t, Block> blocks;
        std::set<std::uint64_t> dirty_blocks;
    };

    std::filesystem::path logical_root;
    std::filesystem::path base_root;
    filesystem::FileSystem * base {nullptr};
    std::map<std::filesystem::path, Node> nodes;
    std::set<std::filesystem::path> directories;

    std::expected<std::filesystem::path, error::Error> relative_path(
        const std::filesystem::path & path
    ) const
    {
        const auto normalized = path.lexically_normal();
        const auto relative = normalized.lexically_relative(logical_root);
        if (!is_within_root(relative)) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::InvalidPath,
                "Overlay path is outside the transaction root",
                "resolve",
                path
            ));
        }
        return relative.lexically_normal();
    }

    std::expected<Node *, error::Error> node(const std::filesystem::path & path)
    {
        auto relative = relative_path(path);
        if (!relative) return std::unexpected(std::move(relative.error()));
        auto [it, inserted] = nodes.try_emplace(*relative);
        auto & value = it->second;
        if (inserted) {
            value.logical_path = logical_root / *relative;
            value.base_path = base_root / *relative;
        }
        if (!value.inspected) {
            auto exists = base->exists(value.base_path);
            if (!exists) return std::unexpected(std::move(exists.error()));
            value.initial_exists = *exists;
            value.exists = *exists;
            if (*exists) {
                auto file = base->open(value.base_path, {
                    filesystem::FileAccess::ReadOnly,
                    filesystem::FileCreateMode::OpenExisting,
                });
                if (!file) return std::unexpected(std::move(file.error()));
                auto size = file->size();
                if (!size) return std::unexpected(std::move(size.error()));
                value.initial_size = *size;
                value.size = *size;
            }
            value.inspected = true;
        }
        return &value;
    }

    std::expected<Block *, error::Error> load_block(Node & node, std::uint64_t block_index)
    {
        if (auto it = node.blocks.find(block_index); it != node.blocks.end()) return &it->second;
        Block block {};
        const auto offset = block_index * BlockSize;
        if (node.initial_exists && offset < node.initial_size) {
            auto file = base->open(node.base_path, {
                filesystem::FileAccess::ReadOnly,
                filesystem::FileCreateMode::OpenExisting,
            });
            if (!file) return std::unexpected(std::move(file.error()));
            auto read = file->read_at(offset, block);
            if (!read) return std::unexpected(std::move(read.error()));
        }
        auto [it, inserted] = node.blocks.emplace(block_index, block);
        return &it->second;
    }

    std::expected<void, error::Error> materialize(Node & node)
    {
        const auto block_count = (node.size + BlockSize - 1) / BlockSize;
        for (std::uint64_t index = 0; index < block_count; ++index) {
            auto block = load_block(node, index);
            if (!block) return std::unexpected(std::move(block.error()));
            node.dirty_blocks.insert(index);
        }
        return {};
    }
};

namespace
{

class OverlayFileHandle final : public filesystem::backend::FileHandleBackend
{
public:
    OverlayFileHandle(
        std::shared_ptr<TransactionFileOverlay::State> state,
        TransactionFileOverlay::State::Node * node,
        filesystem::FileAccess access
    )
        : state_(std::move(state))
        , node_(node)
        , access_(access)
    {
    }

    std::expected<void, error::Error> close() override
    {
        closed_ = true;
        return {};
    }

    std::expected<std::size_t, error::Error> read_at(
        std::uint64_t offset,
        std::span<std::byte> buffer
    ) override
    {
        if (auto valid = validate(false); !valid) return std::unexpected(std::move(valid.error()));
        if (access_ == filesystem::FileAccess::WriteOnly) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::PermissionDenied,
                "Overlay handle is write-only",
                "read_at",
                node_->logical_path
            ));
        }
        if (offset >= node_->size) return std::size_t {0};
        const auto available = static_cast<std::size_t>(
            std::min<std::uint64_t>(buffer.size(), node_->size - offset)
        );
        std::size_t copied {0};
        while (copied < available) {
            const auto absolute = offset + copied;
            const auto block_index = absolute / BlockSize;
            const auto within = static_cast<std::size_t>(absolute % BlockSize);
            const auto count = std::min<std::size_t>(available - copied, BlockSize - within);
            auto block = state_->load_block(*node_, block_index);
            if (!block) return std::unexpected(std::move(block.error()));
            std::copy_n((*block)->begin() + within, count, buffer.begin() + copied);
            copied += count;
        }
        return available;
    }

    std::expected<void, error::Error> write_at(
        std::uint64_t offset,
        std::span<const std::byte> data
    ) override
    {
        if (auto valid = validate(true); !valid) return valid;
        if (access_ == filesystem::FileAccess::ReadOnly) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::ReadOnly,
                "Overlay handle is read-only",
                "write_at",
                node_->logical_path
            ));
        }
        if (data.size() > std::numeric_limits<std::uint64_t>::max() - offset) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::InvalidArgument,
                "Overlay write range overflows",
                "write_at",
                node_->logical_path
            ));
        }
        std::size_t consumed {0};
        while (consumed < data.size()) {
            const auto absolute = offset + consumed;
            const auto block_index = absolute / BlockSize;
            const auto within = static_cast<std::size_t>(absolute % BlockSize);
            const auto count = std::min<std::size_t>(data.size() - consumed, BlockSize - within);
            auto block = state_->load_block(*node_, block_index);
            if (!block) return std::unexpected(std::move(block.error()));
            std::copy_n(data.begin() + consumed, count, (*block)->begin() + within);
            node_->dirty_blocks.insert(block_index);
            consumed += count;
        }
        node_->size = std::max(node_->size, offset + data.size());
        return {};
    }

    std::expected<void, error::Error> append(std::span<const std::byte> data) override
    {
        return write_at(node_->size, data);
    }

    std::expected<std::uint64_t, error::Error> size() override
    {
        if (auto valid = validate(false); !valid) return std::unexpected(std::move(valid.error()));
        return node_->size;
    }

    std::expected<void, error::Error> truncate(std::uint64_t size) override
    {
        if (auto valid = validate(true); !valid) return valid;
        if (access_ == filesystem::FileAccess::ReadOnly) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::ReadOnly,
                "Overlay handle is read-only",
                "truncate",
                node_->logical_path
            ));
        }
        node_->size = size;
        const auto retained_blocks = (size + BlockSize - 1) / BlockSize;
        for (auto it = node_->blocks.lower_bound(retained_blocks); it != node_->blocks.end();) {
            node_->dirty_blocks.erase(it->first);
            it = node_->blocks.erase(it);
        }
        if (size != 0 && size % BlockSize != 0) {
            const auto block_index = size / BlockSize;
            auto block = state_->load_block(*node_, block_index);
            if (!block) return std::unexpected(std::move(block.error()));
            std::fill((*block)->begin() + static_cast<std::ptrdiff_t>(size % BlockSize), (*block)->end(), std::byte {0});
            node_->dirty_blocks.insert(block_index);
        }
        return {};
    }

    std::expected<void, error::Error> sync_data() override { return validate(false); }
    std::expected<void, error::Error> sync_all() override { return validate(false); }

private:
    std::expected<void, error::Error> validate(bool require_exists) const
    {
        if (closed_) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::ClosedHandle,
                "Overlay handle is closed",
                "handle",
                node_->logical_path
            ));
        }
        if (require_exists && !node_->exists) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::NotFound,
                "Overlay file does not exist",
                "handle",
                node_->logical_path
            ));
        }
        return {};
    }

    std::shared_ptr<TransactionFileOverlay::State> state_;
    TransactionFileOverlay::State::Node * node_;
    filesystem::FileAccess access_;
    bool closed_ {false};
};

class OverlayFileSystemBackend final : public filesystem::backend::FileSystemBackend
{
public:
    explicit OverlayFileSystemBackend(std::shared_ptr<TransactionFileOverlay::State> state)
        : state_(std::move(state))
    {
    }

    std::expected<std::unique_ptr<filesystem::backend::FileHandleBackend>, error::Error> open(
        const std::filesystem::path & path,
        const filesystem::FileOpenOptions & options
    ) override
    {
        auto found = state_->node(path);
        if (!found) return std::unexpected(std::move(found.error()));
        auto * node = *found;
        switch (options.create_mode) {
        case filesystem::FileCreateMode::OpenExisting:
            if (!node->exists) return std::unexpected(missing_error(path, "open"));
            break;
        case filesystem::FileCreateMode::OpenOrCreate:
            node->exists = true;
            break;
        case filesystem::FileCreateMode::CreateNew:
            if (node->exists) {
                return std::unexpected(overlay_error(
                    filesystem::FileSystemErrorCode::AlreadyExists,
                    "Overlay file already exists",
                    "open",
                    path
                ));
            }
            node->exists = true;
            node->size = 0;
            node->blocks.clear();
            node->dirty_blocks.clear();
            break;
        case filesystem::FileCreateMode::TruncateExisting:
            if (!node->exists) return std::unexpected(missing_error(path, "open"));
            node->size = 0;
            node->blocks.clear();
            node->dirty_blocks.clear();
            break;
        case filesystem::FileCreateMode::CreateOrTruncate:
            node->exists = true;
            node->size = 0;
            node->blocks.clear();
            node->dirty_blocks.clear();
            break;
        }
        return std::unique_ptr<filesystem::backend::FileHandleBackend>(
            new OverlayFileHandle(state_, node, options.access)
        );
    }

    std::expected<std::vector<std::filesystem::path>, error::Error> list_dir(
        const std::filesystem::path & path
    ) override
    {
        auto relative = state_->relative_path(path);
        if (!relative) return std::unexpected(std::move(relative.error()));
        std::map<std::filesystem::path, bool> entries;
        auto base_entries = state_->base->list_dir(state_->base_root / *relative);
        if (base_entries) {
            for (const auto & entry : *base_entries) {
                entries[state_->logical_root / *relative / entry.filename()] = true;
            }
        } else if (!base_entries.error().is(filesystem::FileSystemErrorCode::NotFound)) {
            return std::unexpected(std::move(base_entries.error()));
        }
        for (auto & [node_relative, node] : state_->nodes) {
            if (node_relative.parent_path() == *relative) {
                entries[node.logical_path] = node.exists;
            }
        }
        std::vector<std::filesystem::path> result;
        for (const auto & [entry, exists] : entries) if (exists) result.push_back(entry.filename());
        return result;
    }

    std::expected<bool, error::Error> exists(const std::filesystem::path & path) override
    {
        auto relative = state_->relative_path(path);
        if (!relative) return std::unexpected(std::move(relative.error()));
        if (state_->directories.contains(*relative)) return true;
        if (auto it = state_->nodes.find(*relative); it != state_->nodes.end() && it->second.inspected) {
            return it->second.exists;
        }
        auto base_exists = state_->base->exists(state_->base_root / *relative);
        if (!base_exists) return std::unexpected(std::move(base_exists.error()));
        return *base_exists;
    }

    std::expected<void, error::Error> create_dir_all(const std::filesystem::path & path) override
    {
        auto relative = state_->relative_path(path);
        if (!relative) return std::unexpected(std::move(relative.error()));
        auto current = *relative;
        while (!current.empty()) {
            state_->directories.insert(current);
            current = current.parent_path();
        }
        return {};
    }

    std::expected<void, error::Error> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override
    {
        auto target_exists = exists(to);
        if (!target_exists) return std::unexpected(std::move(target_exists.error()));
        if (*target_exists) {
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::AlreadyExists,
                "Overlay rename target exists",
                "rename",
                from,
                to
            ));
        }
        return move_file(from, to);
    }

    std::expected<void, error::Error> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override
    {
        return move_file(from, to);
    }

    std::expected<void, error::Error> remove(const std::filesystem::path & path) override
    {
        auto found = state_->node(path);
        if (!found) return std::unexpected(std::move(found.error()));
        if (!(*found)->exists) return {};
        (*found)->exists = false;
        (*found)->size = 0;
        (*found)->blocks.clear();
        (*found)->dirty_blocks.clear();
        return {};
    }

    std::expected<void, error::Error> sync_directory(const std::filesystem::path &) override
    {
        return {};
    }

private:
    std::expected<void, error::Error> move_file(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    )
    {
        auto source = state_->node(from);
        if (!source) return std::unexpected(std::move(source.error()));
        if (!(*source)->exists) return std::unexpected(missing_error(from, "rename"));
        if (auto materialized = state_->materialize(**source); !materialized) return materialized;

        auto target = state_->node(to);
        if (!target) return std::unexpected(std::move(target.error()));
        auto target_initial_exists = (*target)->initial_exists;
        auto target_initial_size = (*target)->initial_size;
        auto target_base_path = (*target)->base_path;
        (*target)->exists = true;
        (*target)->size = (*source)->size;
        (*target)->blocks = std::move((*source)->blocks);
        (*target)->dirty_blocks.clear();
        const auto count = ((*target)->size + BlockSize - 1) / BlockSize;
        for (std::uint64_t index = 0; index < count; ++index) (*target)->dirty_blocks.insert(index);
        (*target)->initial_exists = target_initial_exists;
        (*target)->initial_size = target_initial_size;
        (*target)->base_path = std::move(target_base_path);

        (*source)->exists = false;
        (*source)->size = 0;
        (*source)->blocks.clear();
        (*source)->dirty_blocks.clear();
        return {};
    }

    error::Error missing_error(
        const std::filesystem::path & path,
        std::string operation
    )
    {
        return overlay_error(
            filesystem::FileSystemErrorCode::NotFound,
            "Overlay file does not exist",
            std::move(operation),
            path
        );
    }

    std::shared_ptr<TransactionFileOverlay::State> state_;
};

} // namespace

TransactionFileOverlay::TransactionFileOverlay(
    std::filesystem::path logical_root,
    std::filesystem::path base_root,
    filesystem::FileSystem & base_filesystem
)
    : state_(std::make_shared<State>())
    , filesystem_(std::make_unique<OverlayFileSystemBackend>(state_))
{
    state_->logical_root = std::move(logical_root).lexically_normal();
    state_->base_root = std::move(base_root).lexically_normal();
    state_->base = &base_filesystem;
}

TransactionFileOverlay::~TransactionFileOverlay() = default;

filesystem::FileSystem & TransactionFileOverlay::filesystem() noexcept
{
    return filesystem_;
}

std::expected<wal::FileWriteBatch, error::Error> TransactionFileOverlay::export_batch()
{
    wal::FileWriteBatch batch;
    for (auto & [relative, node] : state_->nodes) {
        if (!node.inspected) continue;
        const auto target = target_for_relative(relative);
        if (!target) {
            if (!node.exists) continue;
            return std::unexpected(overlay_error(
                filesystem::FileSystemErrorCode::InvalidPath,
                "Overlay contains an unsupported final file",
                "export",
                node.logical_path
            ));
        }
        if (!node.exists) {
            if (node.initial_exists) {
                batch.add({
                    .target = *target,
                    .offset = 0,
                    .after_image = {},
                    .mode = wal::FileWriteMode::Delete,
                });
            }
            continue;
        }

        std::optional<wal::FileWrite> pending;
        for (const auto block_index : node.dirty_blocks) {
            const auto offset = block_index * BlockSize;
            if (offset >= node.size) continue;
            auto block = state_->load_block(node, block_index);
            if (!block) return std::unexpected(std::move(block.error()));
            const auto length = static_cast<std::size_t>(
                std::min<std::uint64_t>(BlockSize, node.size - offset)
            );
            std::vector<std::byte> bytes((*block)->begin(), (*block)->begin() + length);

            bool changed = !node.initial_exists || offset + length > node.initial_size;
            if (!changed) {
                std::vector<std::byte> base_bytes(length);
                auto base = state_->base->open(node.base_path, {
                    filesystem::FileAccess::ReadOnly,
                    filesystem::FileCreateMode::OpenExisting,
                });
                if (!base) return std::unexpected(std::move(base.error()));
                auto read = base->read_at(offset, base_bytes);
                if (!read) return std::unexpected(std::move(read.error()));
                changed = *read != length || base_bytes != bytes;
            }
            if (!changed) continue;

            if (pending && pending->offset + pending->after_image.size() == offset) {
                pending->after_image.insert(pending->after_image.end(), bytes.begin(), bytes.end());
            } else {
                if (pending) batch.add(std::move(*pending));
                pending = wal::FileWrite {
                    .target = *target,
                    .offset = offset,
                    .after_image = std::move(bytes),
                    .mode = wal::FileWriteMode::Overwrite,
                };
            }
        }
        if (pending) batch.add(std::move(*pending));
        if (node.size != node.initial_size || !node.initial_exists) {
            batch.add({
                .target = *target,
                .offset = node.size,
                .after_image = {},
                .mode = wal::FileWriteMode::Truncate,
            });
        }
    }
    return batch;
}

} // namespace litedb::core::transaction
