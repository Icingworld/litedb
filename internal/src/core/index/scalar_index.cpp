#include "core/index/scalar_index.hpp"

#include <utility>

namespace litedb::core::index
{

IndexRange::IndexRange(std::optional<IndexBound> lower, std::optional<IndexBound> upper)
    : lower_(std::move(lower))
    , upper_(std::move(upper))
{
}

IndexRange IndexRange::all()
{
    return IndexRange(std::nullopt, std::nullopt);
}

IndexRange IndexRange::closed(ScalarIndexKey lower, ScalarIndexKey upper)
{
    return between(std::move(lower), true, std::move(upper), true);
}

IndexRange IndexRange::between(
    ScalarIndexKey lower,
    bool lower_inclusive,
    ScalarIndexKey upper,
    bool upper_inclusive
)
{
    return IndexRange(
        IndexBound {.key = std::move(lower), .inclusive = lower_inclusive},
        IndexBound {.key = std::move(upper), .inclusive = upper_inclusive}
    );
}

IndexRange IndexRange::lower_bound(ScalarIndexKey key, bool inclusive)
{
    return IndexRange(
        IndexBound {.key = std::move(key), .inclusive = inclusive},
        std::nullopt
    );
}

IndexRange IndexRange::upper_bound(ScalarIndexKey key, bool inclusive)
{
    return IndexRange(
        std::nullopt,
        IndexBound {.key = std::move(key), .inclusive = inclusive}
    );
}

const std::optional<IndexBound> & IndexRange::lower() const noexcept
{
    return lower_;
}

const std::optional<IndexBound> & IndexRange::upper() const noexcept
{
    return upper_;
}

} // namespace litedb::core::index
