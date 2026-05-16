#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simt {

class ByteAddressableMemory {
public:
    explicit ByteAddressableMemory(std::size_t bytes = 0);

    std::size_t size() const { return bytes_.size(); }
    void resize(std::size_t bytes);
    void clear();

    std::uint8_t read8(std::uint32_t address) const;
    void write8(std::uint32_t address, std::uint8_t value);

    std::int32_t read32(std::uint32_t address) const;
    void write32(std::uint32_t address, std::int32_t value);

private:
    std::vector<std::uint8_t> bytes_;
};

} // namespace simt
