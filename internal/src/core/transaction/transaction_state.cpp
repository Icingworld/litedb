#include "core/transaction/transaction_state.hpp"

namespace litedb::core::transaction
{

bool can_transition(TransactionState from, TransactionState to) noexcept
{
    switch (from) {
    case TransactionState::Active:
        return to == TransactionState::Preparing || to == TransactionState::Aborting;
    case TransactionState::Preparing:
        return to == TransactionState::Committing || to == TransactionState::Aborting;
    case TransactionState::Committing:
        return to == TransactionState::Committed;
    case TransactionState::Aborting:
        return to == TransactionState::Aborted;
    case TransactionState::Committed:
        [[fallthrough]];
    case TransactionState::Aborted:
        return false;
    }
    return false;
}

} // namespace litedb::core::transaction
