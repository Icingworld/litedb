#include "core/transaction/transaction_context.hpp"

#include <stdexcept>
#include <type_traits>

namespace
{
using namespace litedb::core::transaction;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main()
{
    static_assert(!std::is_copy_constructible_v<TransactionContext>);
    static_assert(std::is_move_constructible_v<TransactionContext>);
    require(can_transition(TransactionState::Active, TransactionState::Preparing), "active should prepare");
    require(can_transition(TransactionState::Preparing, TransactionState::Committing), "prepare should commit");
    require(can_transition(TransactionState::Committing, TransactionState::Committed), "commit should finish");
    require(!can_transition(TransactionState::Committed, TransactionState::Aborting), "committed cannot abort");
    require(can_transition(TransactionState::Active, TransactionState::Aborting), "active should abort");
    require(can_transition(TransactionState::Aborting, TransactionState::Aborted), "abort should finish");
    TransactionContext context {1};
    require(context.id() == 1 && context.state() == TransactionState::Active, "context defaults mismatch");
    return 0;
}
