#include "memory.hpp"

#include <algorithm>
#include <stdexcept>

namespace simt {

ByteAddressableMemory::ByteAddressableMemory(std::size_t bytes) : bytes_(bytes, 0) {}

void ByteAddressableMemory::resize(std::size_t bytes) {
    bytes_.assign(bytes, 0);
}

void ByteAddressableMemory::clear() {
    std::fill(bytes_.begin(), bytes_.end(), 0);
}

std::uint8_t ByteAddressableMemory::read8(std::uint32_t address) const {
    if (address >= bytes_.size()) {
        throw std::out_of_range("memory read out of range");
    }
    return bytes_[address];
}

void ByteAddressableMemory::write8(std::uint32_t address, std::uint8_t value) {
    if (address >= bytes_.size()) {
        throw std::out_of_range("memory write out of range");
    }
    bytes_[address] = value;
}

std::int32_t ByteAddressableMemory::read32(std::uint32_t address) const {
    if (static_cast<std::size_t>(address) + 3 >= bytes_.size()) {
        throw std::out_of_range("memory read32 out of range");
    }
    std::uint32_t value = 0;
    value |= static_cast<std::uint32_t>(bytes_[address]);
    value |= static_cast<std::uint32_t>(bytes_[address + 1]) << 8;
    value |= static_cast<std::uint32_t>(bytes_[address + 2]) << 16;
    value |= static_cast<std::uint32_t>(bytes_[address + 3]) << 24;
    return static_cast<std::int32_t>(value);
}

void ByteAddressableMemory::write32(std::uint32_t address, std::int32_t value) {
    if (static_cast<std::size_t>(address) + 3 >= bytes_.size()) {
        throw std::out_of_range("memory write32 out of range");
    }
    const auto raw = static_cast<std::uint32_t>(value);
    bytes_[address] = static_cast<std::uint8_t>(raw & 0xffu);
    bytes_[address + 1] = static_cast<std::uint8_t>((raw >> 8) & 0xffu);
    bytes_[address + 2] = static_cast<std::uint8_t>((raw >> 16) & 0xffu);
    bytes_[address + 3] = static_cast<std::uint8_t>((raw >> 24) & 0xffu);
}

} // namespace simt
