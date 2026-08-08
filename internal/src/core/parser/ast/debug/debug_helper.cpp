#include "core/parser/ast/debug/debug_helper.hpp"

namespace litedb::core::parser::ast
{

std::string_view token_type_name(TokenType type) noexcept
{
    switch (type) {
    case TokenType::EoF: return "EoF";
    case TokenType::Select: return "Select";
    case TokenType::Create: return "Create";
    case TokenType::Insert: return "Insert";
    case TokenType::Delete: return "Delete";
    case TokenType::Update: return "Update";
    case TokenType::Drop: return "Drop";
    case TokenType::Use: return "Use";
    case TokenType::Show: return "Show";
    case TokenType::Describe: return "Describe";
    case TokenType::Desc: return "Desc";
    case TokenType::Database: return "Database";
    case TokenType::Collection: return "Collection";
    case TokenType::Index: return "Index";
    case TokenType::VIndex: return "VIndex";
    case TokenType::Databases: return "Databases";
    case TokenType::Collections: return "Collections";
    case TokenType::Indexes: return "Indexes";
    case TokenType::VIndexes: return "VIndexes";
    case TokenType::By: return "By";
    case TokenType::Order: return "Order";
    case TokenType::Asc: return "Asc";
    case TokenType::Limit: return "Limit";
    case TokenType::Offset: return "Offset";
    case TokenType::In: return "In";
    case TokenType::Between: return "Between";
    case TokenType::Like: return "Like";
    case TokenType::Unique: return "Unique";
    case TokenType::Default: return "Default";
    case TokenType::Comment: return "Comment";
    case TokenType::Using: return "Using";
    case TokenType::BTree: return "BTree";
    case TokenType::With: return "With";
    case TokenType::From: return "From";
    case TokenType::Where: return "Where";
    case TokenType::Into: return "Into";
    case TokenType::Values: return "Values";
    case TokenType::Set: return "Set";
    case TokenType::And: return "And";
    case TokenType::Or: return "Or";
    case TokenType::Not: return "Not";
    case TokenType::As: return "As";
    case TokenType::On: return "On";
    case TokenType::If: return "If";
    case TokenType::Exists: return "Exists";
    case TokenType::Null: return "Null";
    case TokenType::True: return "True";
    case TokenType::False: return "False";
    case TokenType::Integer: return "Integer";
    case TokenType::BigInt: return "BigInt";
    case TokenType::Float: return "Float";
    case TokenType::Double: return "Double";
    case TokenType::Varchar: return "Varchar";
    case TokenType::Boolean: return "Boolean";
    case TokenType::Vector: return "Vector";
    case TokenType::Identifier: return "Identifier";
    case TokenType::StringLiteral: return "StringLiteral";
    case TokenType::IntegerLiteral: return "IntegerLiteral";
    case TokenType::FloatLiteral: return "FloatLiteral";
    case TokenType::Equal: return "=";
    case TokenType::NotEqual: return "!=";
    case TokenType::LessThan: return "<";
    case TokenType::GreaterThan: return ">";
    case TokenType::LessEqual: return "<=";
    case TokenType::GreaterEqual: return ">=";
    case TokenType::Plus: return "+";
    case TokenType::Minus: return "-";
    case TokenType::Star: return "*";
    case TokenType::Slash: return "/";
    case TokenType::Modulo: return "%";
    case TokenType::Comma: return ",";
    case TokenType::Semicolon: return ";";
    case TokenType::Dot: return ".";
    case TokenType::LeftParen: return "(";
    case TokenType::RightParen: return ")";
    case TokenType::LeftBracket: return "[";
    case TokenType::RightBracket: return "]";
    case TokenType::Error: return "Error";
    }

    return "Unknown";
}

std::string_view logical_type_name(common::LogicalTypeId id) noexcept
{
    switch (id) {
    case common::LogicalTypeId::Null: return "Null";
    case common::LogicalTypeId::Boolean: return "Boolean";
    case common::LogicalTypeId::Integer: return "Integer";
    case common::LogicalTypeId::BigInt: return "BigInt";
    case common::LogicalTypeId::Float: return "Float";
    case common::LogicalTypeId::Double: return "Double";
    case common::LogicalTypeId::Varchar: return "Varchar";
    case common::LogicalTypeId::Vector: return "Vector";
    }

    return "Unknown";
}

std::string_view create_index_method_name(CreateIndexMethod method) noexcept
{
    switch (method) {
    case CreateIndexMethod::Default: return "Default";
    case CreateIndexMethod::BTree: return "BTree";
    }

    return "Unknown";
}

std::string_view create_vector_index_method_name(CreateVectorIndexMethod method) noexcept
{
    switch (method) {
    case CreateVectorIndexMethod::Hnsw: return "Hnsw";
    }

    return "Unknown";
}

std::string_view vector_index_metric_name(VectorIndexMetric metric) noexcept
{
    switch (metric) {
    case VectorIndexMetric::Default: return "Default";
    case VectorIndexMetric::L2: return "L2";
    case VectorIndexMetric::InnerProduct: return "InnerProduct";
    case VectorIndexMetric::Cosine: return "Cosine";
    }

    return "Unknown";
}

} // namespace litedb::core::parser::ast
