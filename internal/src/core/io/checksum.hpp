#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace litedb::core::io
{

// CRC32 计算器
// 用于多阶段计算校验和，例如文件头和文件主体
//
// 使用方法：
// Crc32Calculator calculator;
// calculator.update(file_header);
// calculator.update(file_body);
// std::uint32_t checksum = calculator.value();
class Crc32Calculator
{
public:
    Crc32Calculator() noexcept;

public:
    // 更新校验和
    void update(std::span<const std::byte> data) noexcept;

    // 获取校验和
    [[nodiscard]]
    std::uint32_t value() const noexcept;

private:
    std::uint32_t checksum_;
};

// 计算单次校验和
[[nodiscard]]
std::uint32_t crc32(std::span<const std::byte> bytes) noexcept;

} // namespace litedb::core::io
