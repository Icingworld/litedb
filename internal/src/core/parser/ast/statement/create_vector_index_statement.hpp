#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

enum class CreateVectorIndexMethod
{
    Hnsw,
};

enum class VectorIndexMetric
{
    Default,
    L2,
    InnerProduct,
    Cosine,
};

struct VectorIndexOptions
{
    VectorIndexMetric metric {VectorIndexMetric::Default};
    std::optional<std::size_t> max_neighbors;
    std::optional<std::size_t> ef_construction;
    std::optional<std::size_t> ef_search;
    std::optional<std::size_t> random_seed;
};

class CreateVectorIndexStatement final : public StatementNode
{
public:
    CreateVectorIndexStatement(
        std::string index_name,
        std::string collection_name,
        std::string column_name,
        bool if_not_exists,
        CreateVectorIndexMethod method,
        VectorIndexOptions options,
        AstNodeLocation location
    ) noexcept;

public:
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & index_name() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    const std::string & column_name() const noexcept;

    [[nodiscard]]
    bool if_not_exists() const noexcept;

    [[nodiscard]]
    CreateVectorIndexMethod method() const noexcept;

    [[nodiscard]]
    const VectorIndexOptions & options() const noexcept;

private:
    std::string index_name_;
    std::string collection_name_;
    std::string column_name_;
    bool if_not_exists_;
    CreateVectorIndexMethod method_;
    VectorIndexOptions options_;
};

} // namespace litedb::core::parser::ast
