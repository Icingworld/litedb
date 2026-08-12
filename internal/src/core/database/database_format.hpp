#pragma once

#include <cstdint>

namespace litedb::core::database
{

inline constexpr std::uint16_t DatabaseFormatVersion = 2;           // 数据库格式版本
inline constexpr std::uint16_t FileHeaderSize = 8;                  // 文件头大小

inline constexpr std::uint32_t ManifestMagic = 0x464d444c;          // LDMF

inline constexpr const char * ManifestFileName = "manifest.ldb";    // manifest 文件名
inline constexpr const char * CatalogFileName = "catalog.lcat";    // catalog 文件名
inline constexpr const char * CollectionsDirName = "collections";   // collections 目录名

} // namespace litedb::core::database
