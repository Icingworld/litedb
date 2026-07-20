#include "core/transaction/transaction_context.hpp"

#include <utility>

namespace litedb::core::transaction
{

TransactionContext::TransactionContext(TransactionId id) noexcept
    : id_(id)
{
}

TransactionId TransactionContext::id() const noexcept
{
    return id_;
}

TransactionState TransactionContext::state() const noexcept
{
    return state_;
}

IsolationLevel TransactionContext::isolation() const noexcept
{
    return isolation_;
}

const std::optional<Lsn> & TransactionContext::first_lsn() const noexcept
{
    return first_lsn_;
}

const std::optional<Lsn> & TransactionContext::last_lsn() const noexcept
{
    return last_lsn_;
}

const std::optional<Lsn> & TransactionContext::commit_lsn() const noexcept
{
    return commit_lsn_;
}

const std::vector<RowMutation> & TransactionContext::write_set() const noexcept
{
    return write_set_;
}

bool TransactionContext::rollback_only() const noexcept
{
    return rollback_only_;
}

const std::optional<TransactionFailure> & TransactionContext::failure() const noexcept
{
    return failure_;
}

void TransactionContext::stage(RowMutation mutation)
{
    write_set_.push_back(std::move(mutation));
}

bool TransactionContext::transition_to(TransactionState next) noexcept
{
    if (!can_transition(state_, next)) {
        return false;
    }
    state_ = next;
    return true;
}

void TransactionContext::note_lsn(Lsn lsn) noexcept
{
    if (!first_lsn_) {
        first_lsn_ = lsn;
    }
    last_lsn_ = lsn;
}

void TransactionContext::note_commit_lsn(Lsn lsn) noexcept
{
    note_lsn(lsn);
    commit_lsn_ = lsn;
}

void TransactionContext::mark_rollback_only(std::string message)
{
    rollback_only_ = true;
    failure_ = TransactionFailure {.message = std::move(message)};
}

void TransactionContext::release_writer_guard() noexcept
{
    if (writer_guard_.owns_lock()) {
        writer_guard_.unlock();
    }
}

} // namespace litedb::core::transaction
