#include "core/parser/parser_worker.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <utility>

#include "core/parser/ast/statement/create_collection_statement.hpp"
#include "core/parser/ast/statement/create_database_statement.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/parser_helper.hpp"

namespace litedb::core::parser
{

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_create_statement()
{
    // 保存并消耗 CREATE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 尝试匹配 DATABASE 关键字
    if (match(TokenType::Database)) {
        // 解析 CREATE DATABASE 语句

        // 判断是否存在 IF NOT EXISTS 关键字
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }

        // 解析数据库名称
        auto database = parse_identifier_string("Expected database name");
        if (!database.has_value()) [[unlikely]] {
            return std::unexpected(database.error());
        }

        return std::make_unique<ast::CreateDatabaseStatement>(
            std::move(database.value()),
            if_not_exists.value(),
            ast_location(location)
        );
    }

    // 尝试匹配 COLLECTION 关键字
    if (match(TokenType::Collection)) {
        // 解析 CREATE COLLECTION 语句

        // 判断是否存在 IF NOT EXISTS 关键字
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }

        // 解析集合名称
        auto collection = parse_identifier_string("Expected collection name");
        if (!collection.has_value()) [[unlikely]] {
            return std::unexpected(collection.error());
        }

        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' before column definitions");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 检查是否为空列表
        if (check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one column definition"));
        }

        // 解析列定义列表
        ast::ColumnDefinitionList columns;
        while (true) {
            auto column = parse_column_definition();
            if (!column.has_value()) [[unlikely]] {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            // 列表元素之间期望使用逗号分隔
            if (!match(TokenType::Comma)) {
                break;
            }
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after column definitions");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        std::optional<std::string> comment;
        if (match(TokenType::Comment)) {
            auto comment_token = consume(TokenType::StringLiteral, "Expected string literal after COMMENT");
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
            ast_location(location)
        );
    }

    // 匹配 INDEX 关键字
    if (match(TokenType::Index)) {
        // 解析 CREATE INDEX 语句

        // 判断是否存在 IF NOT EXISTS 关键字
        auto if_not_exists = parse_if_not_exists();
        if (!if_not_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_not_exists.error());
        }   

        // 解析索引名称
        auto index_name = parse_identifier_string("Expected index name");
        if (!index_name.has_value()) [[unlikely]] {
            return std::unexpected(index_name.error());
        }   

        // 期望 ON 关键字
        auto on = consume(TokenType::On, "Expected ON after index name");
        if (!on.has_value()) [[unlikely]] {
            return std::unexpected(on.error());
        }

        // 解析集合名称
        auto collection_name = parse_identifier_string("Expected collection name");
        if (!collection_name.has_value()) [[unlikely]] {
            return std::unexpected(collection_name.error());
        }

        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' before index column");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }

        // 解析列名称
        auto column_name = parse_identifier_string("Expected index column name");
        if (!column_name.has_value()) [[unlikely]] {
            return std::unexpected(column_name.error());
        }

        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after index column");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }

        // 解析创建索引方法
        auto method = ast::CreateIndexMethod::Default;
        // 尝试匹配 USING 关键字
        if (match(TokenType::Using)) {
            // 解析创建索引方法
            if (match(TokenType::Hash)) {
                method = ast::CreateIndexMethod::Hash;
            } else if (match(TokenType::BTree)) {
                method = ast::CreateIndexMethod::BTree;
            } else [[unlikely]] {
                return std::unexpected(make_parser_error(
                    ParserErrorCode::UnsupportedSyntax,
                    current_token_.location(),
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
            ast_location(location)
        );
    }

    if (match(TokenType::VIndex)) {
        return parse_create_vector_index_statement(location);
    }

    [[unlikely]] return std::unexpected(make_current_error(
        ParserErrorCode::UnsupportedSyntax, 
        "Expected DATABASE, COLLECTION, INDEX, or VINDEX after CREATE"
    ));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_create_vector_index_statement(
    TokenLocation location
)
{
    auto if_not_exists = parse_if_not_exists();
    if (!if_not_exists.has_value()) [[unlikely]] {
        return std::unexpected(if_not_exists.error());
    }

    auto index_name = parse_identifier_string("Expected vector index name");
    if (!index_name.has_value()) [[unlikely]] {
        return std::unexpected(index_name.error());
    }

    auto on = consume(TokenType::On, "Expected ON after vector index name");
    if (!on.has_value()) [[unlikely]] {
        return std::unexpected(on.error());
    }

    auto collection_name = parse_identifier_string("Expected collection name");
    if (!collection_name.has_value()) [[unlikely]] {
        return std::unexpected(collection_name.error());
    }

    auto left_paren = consume(TokenType::LeftParen, "Expected '(' before vector index column");
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(left_paren.error());
    }

    auto column_name = parse_identifier_string("Expected vector index column name");
    if (!column_name.has_value()) [[unlikely]] {
        return std::unexpected(column_name.error());
    }

    auto right_paren = consume(TokenType::RightParen, "Expected ')' after vector index column");
    if (!right_paren.has_value()) [[unlikely]] {
        return std::unexpected(right_paren.error());
    }

    auto using_token = consume(TokenType::Using, "Expected USING HNSW after vector index column");
    if (!using_token.has_value()) [[unlikely]] {
        return std::unexpected(using_token.error());
    }

    auto method = parse_identifier_string("Expected vector index method after USING");
    if (!method.has_value()) [[unlikely]] {
        return std::unexpected(method.error());
    }
    if (lower_ascii(method.value()) != "hnsw") [[unlikely]] {
        return std::unexpected(make_parser_error(
            ParserErrorCode::UnsupportedSyntax,
            current_token_.location(),
            "Expected HNSW after USING"
        ));
    }

    ast::VectorIndexOptions options;
    if (match(TokenType::With)) {
        auto options_left_paren = consume(TokenType::LeftParen, "Expected '(' after WITH");
        if (!options_left_paren.has_value()) [[unlikely]] {
            return std::unexpected(options_left_paren.error());
        }
        if (check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(make_current_error(ParserErrorCode::EmptyList, "Expected at least one vector index option"));
        }

        while (true) {
            auto option_name = parse_identifier_string("Expected vector index option name");
            if (!option_name.has_value()) [[unlikely]] {
                return std::unexpected(option_name.error());
            }
            const auto option_key = lower_ascii(option_name.value());

            auto equal = consume(TokenType::Equal, "Expected '=' after vector index option name");
            if (!equal.has_value()) [[unlikely]] {
                return std::unexpected(equal.error());
            }

            if (option_key == "metric") {
                if (options.metric != ast::VectorIndexMetric::Default) [[unlikely]] {
                    return std::unexpected(make_current_error(ParserErrorCode::UnsupportedSyntax, "Duplicate vector index option: metric"));
                }
                auto metric = parse_identifier_string("Expected vector index metric");
                if (!metric.has_value()) [[unlikely]] {
                    return std::unexpected(metric.error());
                }
                const auto metric_key = lower_ascii(metric.value());
                if (metric_key == "l2") {
                    options.metric = ast::VectorIndexMetric::L2;
                } else if (metric_key == "inner_product") {
                    options.metric = ast::VectorIndexMetric::InnerProduct;
                } else if (metric_key == "cosine") {
                    options.metric = ast::VectorIndexMetric::Cosine;
                } else [[unlikely]] {
                    return std::unexpected(make_parser_error(
                        ParserErrorCode::UnsupportedSyntax,
                        current_token_.location(),
                        "Expected L2, COSINE, or INNER_PRODUCT for vector index metric"
                    ));
                }
            } else if (option_key == "max_neighbors") {
                if (options.max_neighbors.has_value()) [[unlikely]] {
                    return std::unexpected(make_current_error(ParserErrorCode::UnsupportedSyntax, "Duplicate vector index option: max_neighbors"));
                }
                auto value = parse_integer_value("Expected max_neighbors value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.max_neighbors = value.value();
            } else if (option_key == "ef_construction") {
                if (options.ef_construction.has_value()) [[unlikely]] {
                    return std::unexpected(make_current_error(ParserErrorCode::UnsupportedSyntax, "Duplicate vector index option: ef_construction"));
                }
                auto value = parse_integer_value("Expected ef_construction value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.ef_construction = value.value();
            } else if (option_key == "ef_search") {
                if (options.ef_search.has_value()) [[unlikely]] {
                    return std::unexpected(make_current_error(ParserErrorCode::UnsupportedSyntax, "Duplicate vector index option: ef_search"));
                }
                auto value = parse_integer_value("Expected ef_search value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.ef_search = value.value();
            } else if (option_key == "random_seed") {
                if (options.random_seed.has_value()) [[unlikely]] {
                    return std::unexpected(make_current_error(ParserErrorCode::UnsupportedSyntax, "Duplicate vector index option: random_seed"));
                }
                auto value = parse_integer_value("Expected random_seed value");
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(value.error());
                }
                options.random_seed = value.value();
            } else [[unlikely]] {
                return std::unexpected(make_current_error(ParserErrorCode::UnsupportedSyntax, "Unknown vector index option"));
            }

            if (!match(TokenType::Comma)) {
                break;
            }
        }

        auto options_right_paren = consume(TokenType::RightParen, "Expected ')' after vector index options");
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
        ast_location(location)
    );
}

} // namespace litedb::core::parser
