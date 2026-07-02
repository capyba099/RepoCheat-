#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace csdecomp {

class BinaryReader {
public:
    explicit BinaryReader(std::vector<uint8_t> data);

    [[nodiscard]] size_t position() const { return pos_; }
    [[nodiscard]] size_t size() const { return data_.size(); }
    [[nodiscard]] bool eof() const { return pos_ >= data_.size(); }
    [[nodiscard]] const uint8_t* data() const { return data_.data(); }

    void seek(size_t offset);
    void skip(size_t count);

    uint8_t read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();

    std::vector<uint8_t> read_bytes(size_t count);
    std::string read_cstring();

    [[nodiscard]] uint32_t peek_u32(size_t offset = 0) const;

private:
    void ensure_available(size_t count) const;

    std::vector<uint8_t> data_;
    size_t pos_{0};
};

}  // namespace csdecomp
