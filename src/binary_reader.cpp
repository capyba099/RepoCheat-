#include "csdecomp/binary_reader.hpp"

#include <cstring>

namespace csdecomp {

BinaryReader::BinaryReader(std::vector<uint8_t> data) : data_(std::move(data)) {}

void BinaryReader::seek(size_t offset) {
    if (offset > data_.size()) {
        throw std::runtime_error("seek beyond end of stream");
    }
    pos_ = offset;
}

void BinaryReader::skip(size_t count) { seek(pos_ + count); }

void BinaryReader::ensure_available(size_t count) const {
    if (pos_ + count > data_.size()) {
        throw std::runtime_error("unexpected end of stream");
    }
}

uint8_t BinaryReader::read_u8() {
    ensure_available(1);
    return data_[pos_++];
}

uint16_t BinaryReader::read_u16() {
    ensure_available(2);
    uint16_t value{};
    std::memcpy(&value, data_.data() + pos_, 2);
    pos_ += 2;
    return value;
}

uint32_t BinaryReader::read_u32() {
    ensure_available(4);
    uint32_t value{};
    std::memcpy(&value, data_.data() + pos_, 4);
    pos_ += 4;
    return value;
}

uint64_t BinaryReader::read_u64() {
    ensure_available(8);
    uint64_t value{};
    std::memcpy(&value, data_.data() + pos_, 8);
    pos_ += 8;
    return value;
}

std::vector<uint8_t> BinaryReader::read_bytes(size_t count) {
    ensure_available(count);
    std::vector<uint8_t> bytes(count);
    std::memcpy(bytes.data(), data_.data() + pos_, count);
    pos_ += count;
    return bytes;
}

std::string BinaryReader::read_cstring() {
    std::string value;
    while (!eof()) {
        uint8_t ch = read_u8();
        if (ch == 0) {
            break;
        }
        value.push_back(static_cast<char>(ch));
    }
    return value;
}

uint32_t BinaryReader::peek_u32(size_t offset) const {
    ensure_available(offset + 4);
    uint32_t value{};
    std::memcpy(&value, data_.data() + pos_ + offset, 4);
    return value;
}

}  // namespace csdecomp
