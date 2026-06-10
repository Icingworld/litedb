#pragma once

#include "core/catalog/catalog_reader.hpp"
#include "core/catalog/catalog_writer.hpp"

namespace litedb::core::catalog
{

/**
 * @brief 目录
 */
class Catalog : public CatalogReader, public CatalogWriter
{
public:
    ~Catalog() noexcept override = default;
};

} // namespace litedb::core::catalog
