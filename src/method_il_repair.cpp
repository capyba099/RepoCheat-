#include "csdecomp/method_il_repair.hpp"

#include "csdecomp/cli_metadata.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <unordered_set>

namespace csdecomp {

namespace {

constexpr uint32_t kFakeSectionLow = 0x2000U;
constexpr uint32_t kFakeSectionHigh = 0x6000U;
constexpr uint32_t kTextSectionBase = 0x8000U;
constexpr size_t kMaxForwardScan = 128U;

bool is_common_opcode(uint8_t value) {
    static constexpr std::array<uint8_t, 48> kOpcodes = {
        0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x25, 0x26,
        0x27, 0x28, 0x2A, 0x2B, 0x2C, 0x58, 0x59, 0x6F, 0x72, 0x73, 0x74, 0x75, 0x7B, 0x7C,
        0x7D, 0x7E, 0x8C, 0x8D, 0x8E, 0xFE,
    };
    return std::find(kOpcodes.begin(), kOpcodes.end(), value) != kOpcodes.end();
}

}  // namespace

MethodIlRepair::MethodIlRepair(const PeReader& pe, const std::vector<MethodDefRow>& method_defs)
    : pe_(pe) {
    scan_text_bodies();
    low_rva_method_indices_.reserve(method_defs.size());
    for (size_t i = 0; i < method_defs.size(); ++i) {
        const uint32_t rva = method_defs[i].rva;
        if (rva != 0 && rva < kTextSectionBase) {
            low_rva_method_indices_.push_back(i);
        }
    }
}

bool MethodIlRepair::try_read_at(uint32_t rva, std::vector<uint8_t>& out) const {
    if (rva == 0) {
        return false;
    }
    try {
        const size_t max_size = pe_.max_read_size_at_rva(rva);
        if (max_size == 0) {
            return false;
        }
        const size_t peek_size = std::min(max_size, static_cast<size_t>(12));
        const auto header_bytes = pe_.read_at_rva(rva, peek_size);
        const MethodHeader header = parse_method_header(header_bytes, max_size);
        out = pe_.read_at_rva(rva, method_body_total_size(header));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool MethodIlRepair::il_payload_looks_valid(const std::vector<uint8_t>& il) const {
    if (il.empty()) {
        return false;
    }
    try {
        const MethodHeader header = parse_method_header(il, il.size());
        if (header.code_size < 4) {
            return false;
        }
        const size_t code_start = header.header_size;
        const size_t code_end = std::min(il.size(), code_start + std::min(header.code_size, 32U));
        if (code_end <= code_start) {
            return false;
        }
        size_t valid = 0;
        for (size_t i = code_start; i < code_end; ++i) {
            if (is_common_opcode(il[i])) {
                ++valid;
            }
        }
        return valid >= 2;
    } catch (const std::exception&) {
        return false;
    }
}

void MethodIlRepair::scan_text_bodies() {
    uint32_t text_rva = 0;
    size_t text_size = 0;
    for (const auto& section : pe_.sections()) {
        if (section.name.rfind(".text", 0) == 0) {
            text_rva = section.virtual_address;
            text_size = std::max(static_cast<size_t>(section.virtual_size),
                                 static_cast<size_t>(section.raw_data_size));
            break;
        }
    }
    if (text_rva == 0 || text_size < 16) {
        return;
    }

    std::unordered_set<uint32_t> seen;
    for (size_t offset = 0; offset + 12 < text_size; offset += 4) {
        const uint32_t rva = text_rva + static_cast<uint32_t>(offset);
        if (rva < kTextSectionBase || seen.count(rva) != 0) {
            continue;
        }
        std::vector<uint8_t> body;
        if (!try_read_at(rva, body)) {
            continue;
        }
        try {
            const MethodHeader header = parse_method_header(body, body.size());
            if (header.tiny && header.code_size < 12) {
                continue;
            }
            if (!header.tiny && header.code_size < 8) {
                continue;
            }
            if (!il_payload_looks_valid(body)) {
                continue;
            }
            seen.insert(rva);
            text_bodies_.push_back(rva);
            if (method_body_total_size(header) >= 4) {
                offset += method_body_total_size(header) - 4;
            }
        } catch (const std::exception&) {
        }
    }
    std::sort(text_bodies_.begin(), text_bodies_.end());
}

std::vector<uint32_t> MethodIlRepair::candidate_rvas(uint32_t metadata_rva, size_t method_index) const {
    std::vector<uint32_t> candidates;
    auto push_unique = [&](uint32_t rva) {
        if (rva == 0 || rva == metadata_rva) {
            return;
        }
        if (std::find(candidates.begin(), candidates.end(), rva) == candidates.end()) {
            candidates.push_back(rva);
        }
    };

    if (metadata_rva >= kTextSectionBase) {
        for (size_t delta = 0; delta <= kMaxForwardScan; ++delta) {
            push_unique(metadata_rva + static_cast<uint32_t>(delta));
        }
    }

    if (metadata_rva >= kFakeSectionLow && metadata_rva < kFakeSectionHigh) {
        const uint32_t rel = metadata_rva - kFakeSectionLow;
        static constexpr uint32_t kDeltas[] = {0x4e,  0xa4,  0x154, 0x21c, 0x3c0,
                                               0x440, 0x640, 0x80,  0x100, 0x200};
        for (uint32_t delta : kDeltas) {
            push_unique(kTextSectionBase + rel + delta);
            push_unique(kTextSectionBase + rel * 2U + delta);
            push_unique(kTextSectionBase + rel * 4U + delta);
        }
    } else if (metadata_rva >= kFakeSectionHigh && metadata_rva < kTextSectionBase) {
        const uint32_t rel = metadata_rva - kFakeSectionHigh;
        static constexpr uint32_t kDeltas[] = {0x2c, 0xa4, 0x200, 0x400, 0x80};
        for (uint32_t delta : kDeltas) {
            push_unique(kTextSectionBase + rel + delta);
            push_unique(kTextSectionBase + rel * 2U + delta);
        }
    }

    const auto catalog_it =
        std::find(low_rva_method_indices_.begin(), low_rva_method_indices_.end(), method_index);
    if (catalog_it != low_rva_method_indices_.end()) {
        const size_t catalog_index =
            static_cast<size_t>(std::distance(low_rva_method_indices_.begin(), catalog_it));
        if (catalog_index < text_bodies_.size()) {
            push_unique(text_bodies_[catalog_index]);
        }
    }

    return candidates;
}

std::optional<uint32_t> MethodIlRepair::find_repaired_rva(uint32_t metadata_rva,
                                                          size_t method_index) const {
    for (uint32_t candidate : candidate_rvas(metadata_rva, method_index)) {
        std::vector<uint8_t> body;
        if (!try_read_at(candidate, body)) {
            continue;
        }
        if (!il_payload_looks_valid(body)) {
            continue;
        }
        return candidate;
    }
    return std::nullopt;
}

std::vector<uint8_t> MethodIlRepair::read_method_il(uint32_t metadata_rva, size_t method_index) const {
    if (metadata_rva == 0) {
        return {};
    }

    std::vector<uint8_t> direct;
    const bool direct_readable = try_read_at(metadata_rva, direct);
    if (direct_readable && il_payload_looks_valid(direct)) {
        return direct;
    }

    if (direct_readable && metadata_rva >= kTextSectionBase) {
        return direct;
    }

    if (direct_readable) {
        for (int key = 0; key < 256; ++key) {
            std::vector<uint8_t> decrypted = direct;
            for (uint8_t& byte : decrypted) {
                byte = static_cast<uint8_t>(byte ^ static_cast<uint8_t>(key));
            }
            if (!il_payload_looks_valid(decrypted)) {
                continue;
            }
            try {
                parse_method_header(decrypted, decrypted.size());
                return decrypted;
            } catch (const std::exception&) {
            }
        }
    }

    const auto repaired = find_repaired_rva(metadata_rva, method_index);
    if (repaired.has_value()) {
        std::vector<uint8_t> repaired_body;
        if (try_read_at(*repaired, repaired_body)) {
            return repaired_body;
        }
    }

    if (direct_readable) {
        return direct;
    }

    throw std::runtime_error(
        "invalid method header: unknown header format (expected tiny=0x2 or fat=0x3 in low bits)");
}

}  // namespace csdecomp
