#include "memory/common/free_list.hpp"

#include <cassert>

namespace litedb::memory
{

FreeBlock::FreeBlock() noexcept
    : next_(nullptr)
{
}

FreeBlock::FreeBlock(FreeBlock * next) noexcept
    : next_(next)
{
}

FreeBlock * FreeBlock::next() const noexcept
{
    return next_;
}

void FreeBlock::set_next(FreeBlock * next) noexcept
{
    next_ = next;
}

FreeListIterator::FreeListIterator()
    : current_(nullptr)
{
}

FreeListIterator::FreeListIterator(FreeBlock * block) noexcept
    : current_(block)
{
}

FreeListIterator::reference FreeListIterator::operator*() const noexcept
{
    return *current_;
}

FreeListIterator::pointer FreeListIterator::operator->() const noexcept
{
    return current_;
}

FreeListIterator & FreeListIterator::operator++() noexcept
{
    current_ = current_->next();
    return *this;
}

FreeListIterator FreeListIterator::operator++(int) noexcept
{
    FreeListIterator temp = *this;
    ++(*this);
    return temp;
}

bool FreeListIterator::operator==(const FreeListIterator & other) const noexcept
{
    return current_ == other.current_;
}

bool FreeListIterator::operator!=(const FreeListIterator & other) const noexcept
{
    return current_ != other.current_;
}

FreeList::FreeList() noexcept
    : head_()
    , size_(0)
    , capacity_(0)
{
}

FreeBlock * FreeList::front() noexcept
{
    return head_.next();
}

void FreeList::push_front(FreeBlock * block) noexcept
{
    assert(block != nullptr && "FreeList::push_front()不能添加空指针");
    block->set_next(head_.next());
    head_.set_next(block);
    ++size_;
}

void FreeList::pop_front() noexcept
{
    assert(!empty() && "FreeList::pop_front()在空链表中被调用");
    head_.set_next(head_.next()->next());
    --size_;
}

FreeList::iterator FreeList::begin() noexcept
{
    return iterator(head_.next());
}

FreeList::iterator FreeList::end() noexcept
{
    return iterator(nullptr);
}

bool FreeList::empty() const noexcept
{
    return size_ == 0;
}

std::size_t FreeList::capacity() const noexcept
{
    return capacity_;
}

std::size_t FreeList::size() const noexcept
{
    return size_;
}

void FreeList::clear() noexcept
{
    head_.set_next(nullptr);
    size_ = 0;
    capacity_ = 0;
}

void FreeList::set_capacity(std::size_t capacity) noexcept
{
    capacity_ = capacity;
}

} // namespace litedb::memory
