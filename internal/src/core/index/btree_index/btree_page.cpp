#include "core/index/btree_index/btree_page.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace litedb::core::index::btree_index
{

namespace
{

/**
 * @brief 比较两个 B+ 树内部排序键是否相等
 */
[[nodiscard]]
bool equal_entry_keys(const BTreeEntryKey & left, const BTreeEntryKey & right) noexcept
{
    return compare_btree_entry_keys(left, right) == std::strong_ordering::equal;
}

} // namespace

std::strong_ordering compare_btree_entry_keys(
    const BTreeEntryKey & left,
    const BTreeEntryKey & right
) noexcept
{
    const auto key_order = compare_scalar_index_keys(left.key, right.key);
    if (key_order != std::strong_ordering::equal) {
        return key_order;
    }
    return left.record_id <=> right.record_id;
}

bool BTreeEntryKeyLess::operator()(
    const BTreeEntryKey & left,
    const BTreeEntryKey & right
) const noexcept
{
    return compare_btree_entry_keys(left, right) == std::strong_ordering::less;
}

BTreeLeafPage::BTreeLeafPage(
    BTreePageId page_id,
    BTreePageId previous_page_id,
    BTreePageId next_page_id
) noexcept
    : page_id_(page_id)
    , previous_page_id_(previous_page_id)
    , next_page_id_(next_page_id)
{
}

constexpr BTreePageType BTreeLeafPage::type() noexcept
{
    return BTreePageType::Leaf;
}

BTreePageId BTreeLeafPage::page_id() const noexcept
{
    return page_id_;
}

BTreePageId BTreeLeafPage::previous_page_id() const noexcept
{
    return previous_page_id_;
}

BTreePageId BTreeLeafPage::next_page_id() const noexcept
{
    return next_page_id_;
}

void BTreeLeafPage::set_previous_page_id(BTreePageId page_id) noexcept
{
    previous_page_id_ = page_id;
}

void BTreeLeafPage::set_next_page_id(BTreePageId page_id) noexcept
{
    next_page_id_ = page_id;
}

bool BTreeLeafPage::empty() const noexcept
{
    return entries_.empty();
}

std::size_t BTreeLeafPage::size() const noexcept
{
    return entries_.size();
}

const std::vector<BTreeLeafEntry> & BTreeLeafPage::entries() const noexcept
{
    return entries_;
}

std::size_t BTreeLeafPage::lower_bound(const BTreeEntryKey & entry) const noexcept
{
    const auto position = std::lower_bound(entries_.begin(), entries_.end(), entry, BTreeEntryKeyLess {});
    return static_cast<std::size_t>(position - entries_.begin());
}

std::size_t BTreeLeafPage::lower_bound(const ScalarIndexKey & key) const noexcept
{
    const auto position = std::lower_bound(
        entries_.begin(),
        entries_.end(),
        key,
        [](const BTreeLeafEntry & entry, const ScalarIndexKey & value) {
            return compare_scalar_index_keys(entry.key, value) == std::strong_ordering::less;
        }
    );
    return static_cast<std::size_t>(position - entries_.begin());
}

std::size_t BTreeLeafPage::upper_bound(const ScalarIndexKey & key) const noexcept
{
    const auto position = std::upper_bound(
        entries_.begin(),
        entries_.end(),
        key,
        [](const ScalarIndexKey & value, const BTreeLeafEntry & entry) {
            return compare_scalar_index_keys(value, entry.key) == std::strong_ordering::less;
        }
    );
    return static_cast<std::size_t>(position - entries_.begin());
}

bool BTreeLeafPage::contains(const BTreeEntryKey & entry) const noexcept
{
    const auto position = lower_bound(entry);
    return position < entries_.size() && equal_entry_keys(entries_[position], entry);
}

bool BTreeLeafPage::insert(BTreeLeafEntry entry)
{
    const auto position = lower_bound(entry);
    if (position < entries_.size() && equal_entry_keys(entries_[position], entry)) {
        return false;
    }
    entries_.insert(entries_.begin() + static_cast<std::ptrdiff_t>(position), std::move(entry));
    return true;
}

bool BTreeLeafPage::erase(const BTreeEntryKey & entry)
{
    const auto position = lower_bound(entry);
    if (position >= entries_.size() || !equal_entry_keys(entries_[position], entry)) {
        return false;
    }
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(position));
    return true;
}

BTreeInternalPage::BTreeInternalPage(BTreePageId page_id, BTreePageId first_child_id) noexcept
    : page_id_(page_id)
    , first_child_id_(first_child_id)
{
}

constexpr BTreePageType BTreeInternalPage::type() noexcept
{
    return BTreePageType::Internal;
}

BTreePageId BTreeInternalPage::page_id() const noexcept
{
    return page_id_;
}

BTreePageId BTreeInternalPage::first_child_id() const noexcept
{
    return first_child_id_;
}

bool BTreeInternalPage::empty() const noexcept
{
    return entries_.empty();
}

std::size_t BTreeInternalPage::size() const noexcept
{
    return entries_.size();
}

std::size_t BTreeInternalPage::child_count() const noexcept
{
    return entries_.size() + 1;
}

const std::vector<BTreeInternalEntry> & BTreeInternalPage::entries() const noexcept
{
    return entries_;
}

BTreePageId BTreeInternalPage::child_for(const BTreeEntryKey & key) const noexcept
{
    const auto position = std::upper_bound(
        entries_.begin(),
        entries_.end(),
        key,
        [](const BTreeEntryKey & value, const BTreeInternalEntry & entry) {
            return BTreeEntryKeyLess {}(value, entry.separator);
        }
    );
    if (position == entries_.begin()) {
        return first_child_id_;
    }
    return std::prev(position)->right_child_id;
}

std::optional<BTreePageId> BTreeInternalPage::child_at(std::size_t index) const noexcept
{
    if (index == 0) {
        return first_child_id_;
    }
    if (index > entries_.size()) {
        return std::nullopt;
    }
    return entries_[index - 1].right_child_id;
}

std::optional<std::size_t> BTreeInternalPage::find_child(BTreePageId child_id) const noexcept
{
    if (first_child_id_ == child_id) {
        return 0;
    }
    const auto position = std::find_if(
        entries_.begin(),
        entries_.end(),
        [child_id](const BTreeInternalEntry & entry) {
            return entry.right_child_id == child_id;
        }
    );
    if (position == entries_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(position - entries_.begin()) + 1;
}

bool BTreeInternalPage::insert_child_after(
    BTreePageId left_child_id,
    BTreeEntryKey separator,
    BTreePageId right_child_id
)
{
    if (right_child_id == InvalidBTreePageId || find_child(right_child_id).has_value()) {
        return false;
    }
    const auto left_position = find_child(left_child_id);
    if (!left_position.has_value()) {
        return false;
    }

    const auto insert_position = entries_.begin() + static_cast<std::ptrdiff_t>(*left_position);
    const auto inserted = entries_.insert(insert_position, BTreeInternalEntry {
        .separator = std::move(separator),
        .right_child_id = right_child_id,
    });
    if (!separators_are_strictly_ordered()) {
        entries_.erase(inserted);
        return false;
    }
    return true;
}

bool BTreeInternalPage::replace_separator(
    BTreePageId right_child_id,
    BTreeEntryKey separator
)
{
    const auto position = std::find_if(
        entries_.begin(),
        entries_.end(),
        [right_child_id](const BTreeInternalEntry & entry) {
            return entry.right_child_id == right_child_id;
        }
    );
    if (position == entries_.end()) {
        return false;
    }

    auto previous = std::move(position->separator);
    position->separator = std::move(separator);
    if (!separators_are_strictly_ordered()) {
        position->separator = std::move(previous);
        return false;
    }
    return true;
}

bool BTreeInternalPage::erase_child(BTreePageId child_id)
{
    const auto child_position = find_child(child_id);
    if (!child_position.has_value() || entries_.empty()) {
        return false;
    }

    if (*child_position == 0) {
        first_child_id_ = entries_.front().right_child_id;
        entries_.erase(entries_.begin());
    } else {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(*child_position - 1));
    }
    return true;
}

bool BTreeInternalPage::separators_are_strictly_ordered() const noexcept
{
    return std::adjacent_find(
        entries_.begin(),
        entries_.end(),
        [](const BTreeInternalEntry & left, const BTreeInternalEntry & right) {
            return compare_btree_entry_keys(left.separator, right.separator) != std::strong_ordering::less;
        }
    ) == entries_.end();
}

BTreePageType btree_page_type(const BTreePage & page) noexcept
{
    return std::visit([](const auto & value) { return value.type(); }, page);
}

BTreePageId btree_page_id(const BTreePage & page) noexcept
{
    return std::visit([](const auto & value) { return value.page_id(); }, page);
}

} // namespace litedb::core::index::btree_index
