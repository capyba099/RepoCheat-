#pragma once

#include "csdecomp/method_body.hpp"
#include "csdecomp/pe_reader.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace csdecomp {

struct MethodDefRow;

class MethodIlRepair {
public:
    MethodIlRepair(const PeReader& pe, const std::vector<MethodDefRow>& method_defs);

    [[nodiscard]] std::vector<uint8_t> read_method_il(uint32_t metadata_rva, size_t method_index) const;

private:
    [[nodiscard]] bool try_read_at(uint32_t rva, std::vector<uint8_t>& out) const;
    [[nodiscard]] bool il_payload_looks_valid(const std::vector<uint8_t>& il) const;
    [[nodiscard]] std::optional<uint32_t> find_repaired_rva(uint32_t metadata_rva, size_t method_index) const;
    [[nodiscard]] std::vector<uint32_t> candidate_rvas(uint32_t metadata_rva, size_t method_index) const;

    void scan_text_bodies();

    const PeReader& pe_;
    std::vector<uint32_t> text_bodies_;
    std::vector<size_t> low_rva_method_indices_;
};

}  // namespace csdecomp
