#include "memory/common/span_list.hpp"

#include <cassert>
#include <cstdint>

#include "memory/common/common.hpp"

namespace litedb::memory
{

SpanNode::SpanNode() noexcept
    : prev_(nullptr)
    , next_(nullptr)
    , free_list_()
    , page_id_(0)
    , page_count_(0)
    , used_(0)
{
}

SpanNode * SpanNode::prev() const noexcept
{
    return prev_;
}

void SpanNode::set_prev(SpanNode * prev) noexcept
{
    prev_ = prev;
}

SpanNode * SpanNode::next() const noexcept
{
    return next_;
}

void SpanNode::set_next(SpanNode * next) noexcept
{
    next_ = next;
}

FreeList & SpanNode::free_list() noexcept
{
    return free_list_;
}

const FreeList & SpanNode::free_list() const noexcept
{
    return free_list_;
}

std::size_t SpanNode::page_id() const noexcept
{
    return page_id_;
}

void SpanNode::set_page_id(std::size_t page_id) noexcept
{
    page_id_ = page_id;
}

std::size_t SpanNode::page_count() const noexcept
{
    return page_count_;
}

void SpanNode::set_page_count(std::size_t page_count) noexcept
{
    page_count_ = page_count;
}

std::size_t SpanNode::used() const noexcept
{
    return used_;
}

void SpanNode::increment_used() noexcept
{
    ++used_;
}

void SpanNode::decrement_used() noexcept
{
    --used_;
}

std::size_t SpanNode::ptr_to_id(void * pointer) noexcept
{
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(pointer) >> PAGE_SHIFT);
}

SpanListIterator::SpanListIterator()
    : current_(nullptr)
{
}

SpanListIterator::SpanListIterator(SpanNode * node) noexcept
    : current_(node)
{
}

SpanListIterator::reference SpanListIterator::operator*() const noexcept
{
    return *current_;
}

SpanListIterator::pointer SpanListIterator::operator->() const noexcept
{
    return current_;
}

SpanListIterator & SpanListIterator::operator++() noexcept
{
    current_ = current_->next();
    return *this;
}

SpanListIterator SpanListIterator::operator++(int) noexcept
{
    SpanListIterator temp = *this;
    ++(*this);
    return temp;
}

bool SpanListIterator::operator==(const SpanListIterator & other) const noexcept
{
    return current_ == other.current_;
}

bool SpanListIterator::operator!=(const SpanListIterator & other) const noexcept
{
    return current_ != other.current_;
}

SpanList::SpanList() noexcept
    : head_()
    , size_(0)
{
    head_.set_next(&head_);
    head_.set_prev(&head_);
}

SpanNode & SpanList::front() noexcept
{
    assert(!empty() && "SpanList::front()在空链表中被调用");
    return *head_.next();
}

SpanNode & SpanList::back() noexcept
{
    assert(!empty() && "SpanList::back()在空链表中被调用");
    return *head_.prev();
}

void SpanList::push_front(SpanNode * node) noexcept
{
    assert(node != nullptr && "SpanList::push_front()不能添加空指针");
    SpanNode * next = head_.next();
    node->set_next(next);
    node->set_prev(&head_);
    next->set_prev(node);
    head_.set_next(node);
    ++size_;
}

void SpanList::push_back(SpanNode * node) noexcept
{
    assert(node != nullptr && "SpanList::push_back()不能添加空指针");
    SpanNode * prev = head_.prev();
    node->set_next(&head_);
    node->set_prev(prev);
    prev->set_next(node);
    head_.set_prev(node);
    ++size_;
}

void SpanList::pop_front() noexcept
{
    assert(!empty() && "SpanList::pop_front()在空链表中被调用");
    SpanNode * node = head_.next();
    head_.set_next(node->next());
    node->next()->set_prev(&head_);
    node->set_next(nullptr);
    node->set_prev(nullptr);
    --size_;
}

void SpanList::pop_back() noexcept
{
    assert(!empty() && "SpanList::pop_back()在空链表中被调用");
    SpanNode * node = head_.prev();
    head_.set_prev(node->prev());
    node->prev()->set_next(&head_);
    node->set_next(nullptr);
    node->set_prev(nullptr);
    --size_;
}

void SpanList::remove(SpanNode * node) noexcept
{
    assert(node != nullptr && "SpanList::remove()不能移除空指针");
    node->prev()->set_next(node->next());
    node->next()->set_prev(node->prev());
    node->set_next(nullptr);
    node->set_prev(nullptr);
    --size_;
}

SpanList::iterator SpanList::begin() noexcept
{
    return iterator(head_.next());
}

SpanList::iterator SpanList::end() noexcept
{
    return iterator(&head_);
}

bool SpanList::empty() const noexcept
{
    return size_ == 0;
}

std::size_t SpanList::size() const noexcept
{
    return size_;
}

void SpanList::clear() noexcept
{
    head_.set_next(&head_);
    head_.set_prev(&head_);
    size_ = 0;
}

} // namespace litedb::memory
