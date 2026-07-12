#pragma once

#include <cstdint>

namespace litedb::core::persistence
{

inline constexpr std::uint16_t StorageFormatVersion = 2;            ///< 存储格式版本
inline constexpr std::uint16_t FileHeaderSize = 8;                  ///< 文件头大小

inline constexpr std::uint32_t ManifestMagic = 0x464d444c;          ///< LDMF
inline constexpr std::uint32_t RowsMagic = 0x5752444c;              ///< LDRW
inline constexpr std::uint32_t RowRecordMagic = 0x43455252;         ///< RREC

inline constexpr const char * ManifestFileName = "manifest.ldb";    ///< manifest 文件名
inline constexpr const char * MetaFileName = "meta.lmeta";          ///< meta 文件名
inline constexpr const char * CollectionsDirName = "collections";   ///< collections 目录名

} // namespace litedb::core::persistence
