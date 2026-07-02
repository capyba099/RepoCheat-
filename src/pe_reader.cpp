#include "csdecomp/pe_reader.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace csdecomp {

namespace {

constexpr uint32_t kDosSignature = 0x5A4D;       // MZ
constexpr uint32_t kPeSignature = 0x00004550;    // PE\0\0
constexpr uint32_t kCliDirectoryIndex = 14;

}  // namespace

PeReader::PeReader(std::vector<uint8_t> data) : data_(std::move(data)) { parse(); }

void PeReader::parse() {
    if (data_.size() < 64) {
        throw std::runtime_error("file too small to be a PE image");
    }

    BinaryReader reader(data_);
    if (reader.read_u16() != kDosSignature) {
        throw std::runtime_error("invalid DOS signature (expected MZ)");
    }

    reader.seek(0x3C);
    const uint32_t pe_offset = reader.read_u32();
    reader.seek(pe_offset);

    if (reader.read_u32() != kPeSignature) {
        throw std::runtime_error("invalid PE signature");
    }

    const uint16_t machine = reader.read_u16();
    const uint16_t section_count = reader.read_u16();
    (void)machine;
    reader.skip(12);
    const uint16_t optional_header_size = reader.read_u16();
    reader.skip(2);

    const size_t optional_header_start = reader.position();
    const uint16_t magic = reader.read_u16();
    is_pe32_plus_ = (magic == 0x20B);
    if (!is_pe32_plus_ && magic != 0x10B) {
        throw std::runtime_error("unsupported optional header magic");
    }

    const size_t data_directory_count_offset =
        optional_header_start + (is_pe32_plus_ ? 0x6C : 0x5C);
    reader.seek(data_directory_count_offset);
    const uint32_t data_directory_count = reader.read_u32();
    if (data_directory_count <= kCliDirectoryIndex) {
        throw std::runtime_error("PE image does not contain a CLI header directory");
    }

    reader.seek(data_directory_count_offset + 4 + kCliDirectoryIndex * 8);
    const uint32_t cli_rva = reader.read_u32();
    const uint32_t cli_size = reader.read_u32();
    if (cli_rva == 0 || cli_size == 0) {
        throw std::runtime_error("file is not a .NET assembly (missing CLI header)");
    }

    reader.seek(optional_header_start + optional_header_size);
    sections_.reserve(section_count);
    for (uint16_t i = 0; i < section_count; ++i) {
        SectionHeader section{};
        char name[9]{};
        for (int j = 0; j < 8; ++j) {
            name[j] = static_cast<char>(reader.read_u8());
        }
        section.name = name;
        section.virtual_size = reader.read_u32();
        section.virtual_address = reader.read_u32();
        section.raw_data_size = reader.read_u32();
        section.raw_data_offset = reader.read_u32();
        reader.skip(16);
        sections_.push_back(section);
    }

    BinaryReader cli = reader_at_rva(cli_rva);
    cli.skip(8);
    cli_header_.metadata_rva = cli.read_u32();
    cli_header_.metadata_size = cli.read_u32();
    cli_header_.flags = cli.read_u32();
    cli_header_.entry_point_token = cli.read_u32();

    if (cli_header_.metadata_rva == 0) {
        throw std::runtime_error("CLI metadata RVA is zero");
    }
}

uint32_t PeReader::rva_to_offset(uint32_t rva) const {
    for (const auto& section : sections_) {
        const uint32_t section_end = section.virtual_address + std::max(section.virtual_size, section.raw_data_size);
        if (rva >= section.virtual_address && rva < section_end) {
            return section.raw_data_offset + (rva - section.virtual_address);
        }
    }
    throw std::runtime_error("RVA not mapped to any section: 0x" + std::to_string(rva));
}

size_t PeReader::max_read_size_at_rva(uint32_t rva) const {
    for (const auto& section : sections_) {
        const uint32_t section_end = section.virtual_address + std::max(section.virtual_size, section.raw_data_size);
        if (rva >= section.virtual_address && rva < section_end) {
            return section_end - rva;
        }
    }
    throw std::runtime_error("RVA not mapped to any section: 0x" + std::to_string(rva));
}

std::vector<uint8_t> PeReader::read_at_rva(uint32_t rva, size_t size) const {
    const size_t max_size = max_read_size_at_rva(rva);
    if (size > max_size) {
        throw std::runtime_error("read beyond image bounds at RVA 0x" + std::to_string(rva) + " size " +
                                 std::to_string(size));
    }
    const uint32_t offset = rva_to_offset(rva);
    if (offset + size > data_.size()) {
        throw std::runtime_error("read beyond image bounds at RVA 0x" + std::to_string(rva) + " size " +
                                 std::to_string(size));
    }
    return std::vector<uint8_t>(data_.begin() + offset, data_.begin() + offset + size);
}

BinaryReader PeReader::reader_at_rva(uint32_t rva) const {
    const uint32_t offset = rva_to_offset(rva);
    return BinaryReader(std::vector<uint8_t>(data_.begin() + offset, data_.end()));
}

}  // namespace csdecomp
