#include "core/parser/worker/parser_create_worker.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <utility>

#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/parser_helper.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserCreateWorker::ParserCreateWorker(ParserContext & context)
    : context_(context)
    , schema_helper_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserCreateWorker::parse_create_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    if (context_.match(TokenType::Database)) {

        auto if_not_exists = schema_helper_.parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }

        auto database = schema_helper_.parse_identifier_string("Expected database name");
        if (!database.has_value()) [[unlikely]] {
            return std::unexpected(database.error());
        }

        return std::make_unique<ast::CreateDatabaseStatement>(
            std::move(database.value()),
            if_not_exists.value(),
            context_.ast_location(location)
        );
    }

    if (context_.match(TokenType::Collection)) {

        auto if_not_exists = schema_helper_.parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }

        auto collection = schema_helper_.parse_identifier_string("Expected collection name");
        if (!collection.has_value()) [[unlikely]] {
            return std::unexpected(collection.error());
        }

        auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' before column definitions");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        if (context_.check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(context_.make_current_error(ParserErrorCode::EmptyList, "Expected at least one column definition"));
        }

        ast::ColumnDefinitionList columns;
        while (true) {
            auto column = schema_helper_.parse_column_definition();
            if (!column.has_value()) [[unlikely]] {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }

        auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after column definitions");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        std::optional<std::string> comment;
        if (context_.match(TokenType::Comment)) {
            auto comment_token = context_.consume(TokenType::StringLiteral, "Expected string literal after COMMENT");
            if (!comment_token.has_value()) [[unlikely]] {
                return std::unexpected(comment_token.error());
            }
            comment = std::string(comment_token->value());
        }

        return std::make_unique<ast::CreateCollectionStatement>(
            std::move(collection.value()),
            if_not_exists.value(),
            std::move(columns),
            std::move(comment),
            context_.ast_location(location)
        );
    }

    if (context_.match(TokenType::Index)) {

        auto if_not_exists = schema_helper_.parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }

        auto index_name = schema_helper_.parse_identifier_string("Expected index name");
        if (!index_name.has_value()) [[unlikely]] {
            return std::unexpected(index_name.error());
        }

        auto on = context_.consume(TokenType::On, "Expected ON after index name");
        if (!on.has_value()) [[unlikely]] {
            return std::unexpected(on.error());
        }

        auto collection_name = schema_helper_.parse_identifier_string("Expected collection name");
        if (!collection_name.has_value()) [[unlikely]] {
            return std::unexpected(collection_name.error());
        }

        auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' before index column");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }

        auto column_name = schema_helper_.parse_identifier_string("Expected index column name");
        if (!column_name.has_value()) [[unlikely]] {
            return std::unexpected(column_name.error());
        }

        auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after index column");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        auto method = ast::CreateIndexMethod::Default;
        if (context_.match(TokenType::Using)) {
            if (context_.match(TokenType::Hash)) {
                method = ast::CreateIndexMethod::Hash;
            } else if (context_.match(TokenType::BTree)) {
                method = ast::CreateIndexMethod::BTree;
            } else [[unlikely]] {
                return std::unexpected(make_parser_error(
                    ParserErrorCode::UnsupportedSyntax,
                    context_.current().location(),
                    "Expected HASH or BTREE after USING"
                ));
            }
        }

        return std::make_unique<ast::CreateIndexStatement>(
            std::move(index_name.value()),
            std::move(collection_name.value()),
            std::move(column_name.value()),
            if_not_exists.value(),
            method,
            context_.ast_location(location)
        );
    }

    if (context_.match(TokenType::VIndex)) {
        return parse_create_vector_index_statement(location);
    }

    [[unlikely]] return std::unexpected(context_.make_current_error(
        ParserErrorCode::UnsupportedSyntax,
        "Expected DATABASE, COLLECTION, INDEX, or VINDEX after CREATE"
    ));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserCreateWorker::parse_create_vector_index_statement(
    TokenLocation location
)
{
    auto if_not_exists = schema_helper_.parse_if_not_exists();
    if (!if_not_exists.has_value()) [[unlikely]] {
        return std::unexpected(if_not_exists.error());
    }

    auto index_name = schema_helper_.parse_identifier_string("Expected vector index name");
    if (!index_name.has_value()) [[unlikely]] {
        return std::unexpected(index_name.error());
    }

    auto on = context_.consume(TokenType::On, "Expected ON after vector index name");
    if (!on.has_value()) [[unlikely]] {
        return std::unexpected(on.error());
    }

    auto collection_name = schema_helper_.parse_identifier_string("Expected collection name");
    if (!collection_name.has_value()) [[unlikely]] {
        return std::unexpected(collection_name.error());
    }

    auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' before vector index column");
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(left_paren.error());
    }

    auto column_name = schema_helper_.parse_identifier_string("Expected vector index column name");
    if (!column_name.has_value()) [[unlikely]] {
        return std::unexpected(column_name.error());
    }

    auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after vector index column");
    if (!right_paren.has_value()) [[unlikely]] {
        return std::unexpected(right_paren.error());
    }

    auto using_token = context_.consume(TokenType::Using, "Expected USING HNSW after vector index column");
    if (!using_token.has_value()) [[unlikely]] {
        return std::unexpected(using_token.error());
    }

    auto method_token = context_.consume(
        TokenType::Identifier,
        "Expected vector index method after USING",
        ParserErrorCode::ExpectedIdentifier
    );
    if (!method_token.has_value()) [[unlikely]] {
        return std::unexpected(method_token.error());
    }
    if (lower_ascii(method_token->value()) != "hnsw") [[unlikely]] {
        return std::unexpected(make_parser_error(
            ParserErrorCode::UnsupportedSyntax,
            method_token->location(),
            "Expected HNSW after USING"
        ));
    }

    ast::VectorIndexOptions options;
    if (context_.match(TokenType::With)) {
        auto options_left_paren = context_.consume(TokenType::LeftParen, "Expected '(' after WITH");
        if (!options_left_paren.has_value()) [[unlikely]] {
            return std::unexpected(options_left_paren.error());
        }
        if (context_.check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(context_.make_current_error(ParserErrorCode::EmptyList, "Expected at least one vector index option"));
        }

        while (true) {
            auto option_name = context_.consume(
                TokenType::Identifier,
                "Expected vector index option name",
                ParserErrorCode::ExpectedIdentifier
            );
            if (!option_name.has_value()) [[unlikely]] {
                return std::unexpected(option_name.error());
            }
            const auto option_key = lower_ascii(option_name->value());

            auto equal = context_.consume(TokenType::Equal, "Expected '=' after vector index option name");
            if (!equal.has_value()) [[unlikely]] {
                return std::unexpected(equal.error());
            }

            if (option_key == "metric") {
                if (options.metric != ast::VectorIndexMetric::Default) [[unlikely]] {
                    return std::unexpected(make_parser_error(
                        ParserErrorCode::UnsupportedSyntax,
                        option_name->location(),
                        "Duplicate vector index option: metric"
                    ));
                }
                auto metric_token = context_.consume(
                    TokenType::Identifier,
                    "Expected vector index metric",
                    ParserErrorCode::ExpectedIdentifier
                );
                if (!metric_token.has_value()) [[unlikely]] {
                    return std::unexpected(metric_token.error());
                }
                const auto metric_key = lower_ascii(metric_token->value());
                if (metric_key == "l2") {
                    options.metric = ast::VectorIndexMetric::L2;
                } else if (metric_key == "inner_product") {
                    options.metric = ast::VectorIndexMetric::InnerProduct;
                } else if (metric_key == "cosine") {
                    options.metric = ast::VectorIndexMetric::Cosine;
                } else [[unlikely]] {
                    return std::unexpected(make_parser_error(
                        ParserErrorCode::UnsupportedSyntax,
                        metric_token->location(),
                        "Expected L2, COSINE, or INNER_PRODUCT for vector index metric"
                    ));
                }
            } else if (option_key == "max_neighbors") {
                if (options.max_neighbors.has_value()) [[unlikely]] {
                    return std::unexpected(make_parser_error(
                        ParserErrorCode::UnsupportedSyntax,
                        option_name->location(),
                        "Duplicate vector index option: max_neighbors"
                    ));
                }
                auto value = schema_helper_.parse_integer_value("Expected max_neighbors value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.max_neighbors = value.value();
            } else if (option_key == "ef_construction") {
                if (options.ef_construction.has_value()) [[unlikely]] {
                    return std::unexpected(make_parser_error(
                        ParserErrorCode::UnsupportedSyntax,
                        option_name->location(),
                        "Duplicate vector index option: ef_construction"
                    ));
                }
                auto value = schema_helper_.parse_integer_value("Expected ef_construction value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.ef_construction = value.value();
            } else if (option_key == "ef_search") {
                if (options.ef_search.has_value()) [[unlikely]] {
                    return std::unexpected(make_parser_error(
                        ParserErrorCode::UnsupportedSyntax,
                        option_name->location(),
                        "Duplicate vector index option: ef_search"
                    ));
                }
                auto value = schema_helper_.parse_integer_value("Expected ef_search value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.ef_search = value.value();
            } else if (option_key == "random_seed") {
                if (options.random_seed.has_value()) [[unlikely]] {
                    return std::unexpected(make_parser_error(
                        ParserErrorCode::UnsupportedSyntax,
                        option_name->location(),
                        "Duplicate vector index option: random_seed"
                    ));
                }
                auto value = schema_helper_.parse_integer_value("Expected random_seed value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.random_seed = value.value();
            } else [[unlikely]] {
                return std::unexpected(make_parser_error(
                    ParserErrorCode::UnsupportedSyntax,
                    option_name->location(),
                    "Unknown vector index option"
                ));
            }

            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }

        auto options_right_paren = context_.consume(TokenType::RightParen, "Expected ')' after vector index options");
        if (!options_right_paren.has_value()) [[unlikely]] {
            return std::unexpected(options_right_paren.error());
        }
    }

    return std::make_unique<ast::CreateVectorIndexStatement>(
        std::move(index_name.value()),
        std::move(collection_name.value()),
        std::move(column_name.value()),
        if_not_exists.value(),
        ast::CreateVectorIndexMethod::Hnsw,
        options,
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser
