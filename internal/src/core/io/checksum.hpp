#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace litedb::core::io
{

// 当前使用 CRC32 来计算校验和
// 计算 CRC32 有多种方式，如下：
// 1. bit-by-bit 计算，极慢，不做考虑
// 2. 256 查表法，速度快，需要占用 1 kb 内存，为当前实现方式
// 3. slicing by 4 计算，速度更快，需要占用 4 kb 内存
// 4. slicing by 8 计算，速度更快，需要占用 8 kb 内存
// 5. SIMD 计算，速度很快，实现复杂度高
// 6. 专用 CPU 指令集，速度最快，实现难度较大，难以跨平台通用

// CRC32 计算器
// 用于多阶段计算校验和，例如文件头和文件主体
//
// 使用方法：
// Crc32Calculator calculator;
// calculator.update(file_header);
// calculator.update(file_body);
// std::uint32_t checksum = calculator.value();
//
// 本项目的代码风格为头文件和实现文件分离，如果需要编译期
// 计算，则需要把成员函数 声明为 constexpr，
// 但 constexpr 隐含了 inline 属性，这代表其他翻译单元
// 在阅读此头文件时，只能看到声明，但看不到实现，从而无法
// 在编译期计算校验和。为了这个功能破坏项目的代码风格不值得，
// 因此不将成员函数声明为 constexpr
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
