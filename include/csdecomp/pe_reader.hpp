#pragma once

#include "csdecomp/binary_reader.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace csdecomp {

struct CliHeader {
    uint32_t metadata_rva{0};
    uint32_t metadata_size{0};
    uint32_t flags{0};
    uint32_t entry_point_token{0};
};

struct SectionHeader {
    std::string name;
    uint32_t virtual_address{0};
    uint32_t virtual_size{0};
    uint32_t raw_data_offset{0};
    uint32_t raw_data_size{0};
};

class PeReader {
public:
    explicit PeReader(std::vector<uint8_t> data);

    [[nodiscard]] const std::vector<uint8_t>& raw() const { return data_; }
    [[nodiscard]] const CliHeader& cli_header() const { return cli_header_; }
    [[nodiscard]] const std::vector<SectionHeader>& sections() const { return sections_; }

    [[nodiscard]] uint32_t rva_to_offset(uint32_t rva) const;
    [[nodiscard]] std::vector<uint8_t> read_at_rva(uint32_t rva, size_t size) const;
    [[nodiscard]] BinaryReader reader_at_rva(uint32_t rva) const;

private:
    void parse();

    std::vector<uint8_t> data_;
    CliHeader cli_header_{};
    std::vector<SectionHeader> sections_;
    bool is_pe32_plus_{false};
};

}  // namespace csdecomp
