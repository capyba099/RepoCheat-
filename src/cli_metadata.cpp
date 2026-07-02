#include "csdecomp/cli_metadata.hpp"

#include "csdecomp/method_body.hpp"

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace csdecomp {

namespace {

constexpr int kMaxTypeSignatureDepth = 48;
constexpr uint32_t kMaxMetadataTableRows = 0x001FFFFFU;
constexpr uint32_t kMaxRidListSpan = 8192U;

bool safe_row_range(size_t row_offset, uint32_t row_size, size_t table_size) {
    if (row_size == 0 || row_offset > table_size) {
        return false;
    }
    return row_size <= table_size - row_offset;
}

bool is_plausible_metadata_name(const std::string& name) {
    if (name.empty() || name.size() > 512) {
        return false;
    }
    for (unsigned char ch : name) {
        if (ch < 0x20 || ch == 0x7F) {
            return false;
        }
    }
    return true;
}

void append_rid_list(uint32_t start, uint32_t end, uint32_t max_rid, std::vector<size_t>& out) {
    if (start == 0 || end < start) {
        return;
    }
    const uint32_t span = end - start;
    if (span > kMaxRidListSpan || end > max_rid + 1U) {
        return;
    }
    out.reserve(out.size() + span);
    for (uint32_t rid = start; rid < end; ++rid) {
        out.push_back(static_cast<size_t>(rid - 1U));
    }
}

uint32_t read_compressed_uint(const uint8_t* data, size_t size, size_t& offset) {
    if (offset >= size) {
        throw std::runtime_error("unexpected end of blob while reading compressed uint");
    }
    uint8_t first = data[offset++];
    if ((first & 0x80) == 0) {
        return first;
    }
    if ((first & 0xC0) == 0x80) {
        if (offset >= size) {
            throw std::runtime_error("truncated compressed uint");
        }
        return ((first & 0x3F) << 8) | data[offset++];
    }
    if (offset + 3 > size) {
        throw std::runtime_error("truncated compressed uint");
    }
    uint32_t value = ((first & 0x1F) << 24) | (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
    offset += 3;
    return value;
}

constexpr uint32_t kMaxRowDrift = 15U;

uint32_t read_row_index(const uint8_t* row, size_t offset, uint32_t size) {
    uint32_t value = 0;
    if (size == 2) {
        std::memcpy(&value, row + offset, 2);
    } else {
        std::memcpy(&value, row + offset, 4);
    }
    return value;
}

}  // namespace

int CliMetadata::score_typedef_row_bytes(const uint8_t* row, uint32_t row_size) const {
    if (row == nullptr || row_size < 12) {
        return -1;
    }
    uint32_t flags = 0;
    std::memcpy(&flags, row, 4);
    if (flags > 0x00FFFFFFU) {
        return -1;
    }

    const uint32_t name_index = read_row_index(row, 4, string_index_size_);
    const uint32_t ns_index = read_row_index(row, 4 + string_index_size_, string_index_size_);
    const std::string name = get_string(name_index);
    const std::string ns = get_string(ns_index);

    int score = 0;
    if (name.rfind("Olesya.", 0) == 0 && name.size() >= 10) {
        score += 10;
    } else if (name == "Forms" || name == "Linq" || name == "Text") {
        score += 8;
    } else if (!name.empty() && is_plausible_metadata_name(name)) {
        score += 3;
    }
    if (name.rfind("Olesya.E642B0AD", 0) == 0) {
        score += 50;
    }
    if (ns.rfind("Olesya.E069A6E5", 0) == 0 && name.rfind("Olesya.E642B0AD", 0) == 0) {
        score += 100;
    }
    return score;
}

int CliMetadata::score_methoddef_row_bytes(const uint8_t* row, uint32_t row_size) const {
    if (row == nullptr || row_size < 10) {
        return -1;
    }

    uint32_t rva = 0;
    std::memcpy(&rva, row, 4);
    const uint32_t name_index = read_row_index(row, 8, string_index_size_);
    const std::string name = get_string(name_index);

    int score = 0;
    if (name.rfind("Olesya.", 0) == 0) {
        score += 8;
    }
    if (name == "Forms" || name == "Linq" || name == "Text") {
        score += 15;
    }
    if (name == ".ctor" || name == ".cctor") {
        score += 6;
    }
    if (!name.empty() && is_plausible_metadata_name(name)) {
        score += 2;
    }

    if (rva != 0) {
        try {
            const size_t max_size = pe_.max_read_size_at_rva(rva);
            if (max_size > 0) {
                const size_t peek_size = std::min(max_size, static_cast<size_t>(12));
                const auto header_bytes = pe_.read_at_rva(rva, peek_size);
                parse_method_header(header_bytes, max_size);
                score += 25;
            }
        } catch (const std::exception&) {
        }
    } else if (!name.empty()) {
        score += 1;
    }
    return score;
}

void CliMetadata::read_typedef_rows_standard(size_t row_count, uint32_t row_size, size_t base) {
    const auto read_index = [&](const uint8_t* row, size_t& pos, uint32_t size) -> uint32_t {
        uint32_t value = 0;
        if (size == 2) {
            std::memcpy(&value, row + pos, 2);
        } else {
            std::memcpy(&value, row + pos, 4);
        }
        pos += size;
        return value;
    };

    type_defs_.reserve(row_count);
    for (size_t row = 0; row < row_count; ++row) {
        const size_t row_offset = base + row * row_size;
        if (!safe_row_range(row_offset, row_size, valid_tables_.size())) {
            throw std::runtime_error("metadata row read out of bounds in TypeDef table");
        }
        const uint8_t* row_data = valid_tables_.data() + row_offset;
        size_t pos = 0;
        TypeDefRow entry{};
        entry.flags = read_index(row_data, pos, 4);
        entry.type_name_index = read_index(row_data, pos, string_index_size_);
        entry.type_namespace_index = read_index(row_data, pos, string_index_size_);
        entry.extends_index =
            read_index(row_data, pos, coded_index_size(max_row_count({2, 1, 27}), 2));
        entry.field_list_index = read_index(row_data, pos, simple_index_size(row_counts_[4]));
        entry.method_list_index = read_index(row_data, pos, simple_index_size(row_counts_[6]));
        type_defs_.push_back(entry);
    }
}

void CliMetadata::read_methoddef_rows_standard(size_t row_count, uint32_t row_size, size_t base) {
    const auto read_index = [&](const uint8_t* row, size_t& pos, uint32_t size) -> uint32_t {
        uint32_t value = 0;
        if (size == 2) {
            std::memcpy(&value, row + pos, 2);
        } else {
            std::memcpy(&value, row + pos, 4);
        }
        pos += size;
        return value;
    };

    method_defs_.reserve(row_count);
    for (size_t row = 0; row < row_count; ++row) {
        const size_t row_offset = base + row * row_size;
        if (!safe_row_range(row_offset, row_size, valid_tables_.size())) {
            throw std::runtime_error("metadata row read out of bounds in MethodDef table");
        }
        const uint8_t* row_data = valid_tables_.data() + row_offset;
        size_t pos = 0;
        MethodDefRow entry{};
        entry.rva = read_index(row_data, pos, 4);
        entry.impl_flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
        entry.flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
        entry.name_index = read_index(row_data, pos, string_index_size_);
        entry.signature_index = read_index(row_data, pos, blob_index_size_);
        entry.param_list_index = read_index(row_data, pos, simple_index_size(row_counts_[8]));
        method_defs_.push_back(entry);
    }
}

size_t CliMetadata::find_table_skew(
    size_t base, size_t row_count, uint32_t row_size,
    const std::function<int(const uint8_t*, uint32_t)>& score_row) const {
    size_t best_skew = 0;
    int best_score = -1;
    constexpr size_t kMaxTableSkew = 256;
    for (size_t skew = 0; skew < kMaxTableSkew; ++skew) {
        int total = 0;
        bool valid = true;
        for (size_t row = 0; row < row_count; ++row) {
            const size_t offset = base + skew + row * row_size;
            if (!safe_row_range(offset, row_size, valid_tables_.size())) {
                valid = false;
                break;
            }
            total += score_row(valid_tables_.data() + offset, row_size);
        }
        if (!valid) {
            continue;
        }
        if (total > best_score) {
            best_score = total;
            best_skew = skew;
        }
    }
    return best_skew;
}

void CliMetadata::read_typedef_rows_with_skew(size_t row_count, uint32_t row_size, size_t base) {
    const size_t skew = find_table_skew(
        base, row_count, row_size,
        [this](const uint8_t* row, uint32_t size) { return score_typedef_row_bytes(row, size); });
    read_typedef_rows_standard(row_count, row_size, base + skew);
}

void CliMetadata::read_methoddef_rows_with_skew(size_t row_count, uint32_t row_size, size_t base) {
    const size_t skew = find_table_skew(
        base, row_count, row_size,
        [this](const uint8_t* row, uint32_t size) { return score_methoddef_row_bytes(row, size); });
    read_methoddef_rows_standard(row_count, row_size, base + skew);
}

CliMetadata::CliMetadata(const PeReader& pe) : pe_(pe) {
    parse_metadata_root();
    read_tables_header();

    uint8_t best_heap = static_cast<uint8_t>(tables_heap_sizes_declared_ & 0x07);
    size_t best_score = 0;
    const bool declared_heap_trusted = (tables_heap_sizes_declared_ & 0xF8U) == 0;
    for (int mask = 0; mask < 8; ++mask) {
        const uint8_t heap_sizes = static_cast<uint8_t>(mask & 0x07);
        clear_parsed_tables();
        if (!parse_tables_with_heap_sizes(heap_sizes)) {
            continue;
        }
        size_t score = score_metadata_layout();
        if (declared_heap_trusted && heap_sizes == (tables_heap_sizes_declared_ & 0x07)) {
            score += 50;
        }
        if (score > best_score) {
            best_score = score;
            best_heap = heap_sizes;
        }
    }

    clear_parsed_tables();
    if (!parse_tables_with_heap_sizes(best_heap)) {
        throw std::runtime_error("failed to parse metadata tables");
    }
}

void CliMetadata::read_tables_header() {
    BinaryReader reader(tables_data_);
    reader.skip(4);
    reader.skip(2);
    tables_heap_sizes_declared_ = reader.read_u8();
    obfuscated_metadata_ = (tables_heap_sizes_declared_ & 0xF8U) != 0;
    reader.skip(1);
    tables_valid_ = reader.read_u64();
    reader.skip(8);

    row_counts_.assign(64, 0);
    for (int i = 0; i < 64; ++i) {
        if (((tables_valid_ >> i) & 1ULL) != 0) {
            row_counts_[i] = reader.read_u32();
        }
    }
    tables_start_ = reader.position();
}

void CliMetadata::clear_parsed_tables() {
    method_defs_.clear();
    type_defs_.clear();
    fields_.clear();
    params_.clear();
    member_refs_.clear();
    type_refs_.clear();
    type_specs_.clear();
    nested_classes_.clear();
    field_ptrs_.clear();
    method_ptrs_.clear();
    param_ptrs_.clear();
    properties_.clear();
    property_maps_.clear();
    valid_tables_.clear();
    table_row_sizes_.clear();
    table_offsets_.clear();
}

bool CliMetadata::has_table(TableId id) const {
    const int table_index = static_cast<int>(id);
    return table_index >= 0 && table_index < 64 && ((tables_valid_ >> table_index) & 1ULL) != 0;
}

size_t CliMetadata::score_metadata_layout() const {
    size_t score = 0;

    for (const auto& type : type_defs_) {
        const std::string name = get_string(type.type_name_index);
        const std::string ns = get_string(type.type_namespace_index);
        if (is_plausible_metadata_name(name)) {
            score += 8;
            if (name.rfind("Olesya.", 0) == 0 && name.size() >= 10) {
                score += 12;
            }
        }
        if (is_plausible_metadata_name(ns)) {
            score += 4;
            if (ns.rfind("Olesya.", 0) == 0 && ns.size() >= 10) {
                score += 8;
            }
        }
        const std::string base = resolve_type_name(type.extends_index);
        if (!base.empty() && base.find('?') == std::string::npos) {
            score += 2;
        }
    }

    for (const auto& type_ref : type_refs_) {
        const std::string name = get_string(type_ref.type_name_index);
        if (is_plausible_metadata_name(name)) {
            score += 1;
        }
    }

    size_t checked_methods = 0;
    for (const auto& method : method_defs_) {
        if (method.rva == 0) {
            continue;
        }
        ++checked_methods;
        try {
            const size_t max_size = pe_.max_read_size_at_rva(method.rva);
            if (max_size == 0) {
                continue;
            }
            const size_t peek_size = std::min(max_size, static_cast<size_t>(12));
            const auto header_bytes = pe_.read_at_rva(method.rva, peek_size);
            parse_method_header(header_bytes, max_size);
            score += 120;
            const std::string name = get_string(method.name_index);
            if (is_plausible_metadata_name(name)) {
                score += 6;
            }
        } catch (const std::exception&) {
        }
        if (checked_methods >= 256) {
            break;
        }
    }

    if (!method_defs_.empty() && checked_methods == 0) {
        score /= 4;
    }
    return score;
}

uint32_t CliMetadata::resolve_field_rid(uint32_t list_index) const {
    if (list_index == 0) {
        return 0;
    }
    if (has_table(TableId::FieldPtr)) {
        if (list_index > field_ptrs_.size()) {
            return 0;
        }
        return field_ptrs_[list_index - 1];
    }
    return list_index;
}

uint32_t CliMetadata::resolve_method_rid(uint32_t list_index) const {
    if (list_index == 0) {
        return 0;
    }
    if (has_table(TableId::MethodPtr)) {
        if (list_index > method_ptrs_.size()) {
            return 0;
        }
        return method_ptrs_[list_index - 1];
    }
    return list_index;
}

uint32_t CliMetadata::resolve_param_rid(uint32_t list_index) const {
    if (list_index == 0) {
        return 0;
    }
    if (has_table(TableId::ParamPtr)) {
        if (list_index > param_ptrs_.size()) {
            return 0;
        }
        return param_ptrs_[list_index - 1];
    }
    return list_index;
}

bool CliMetadata::parse_tables_with_heap_sizes(uint8_t heap_sizes) {
    try {
        string_index_size_ = (heap_sizes & 0x01) != 0 ? 4U : 2U;
        blob_index_size_ = (heap_sizes & 0x02) != 0 ? 4U : 2U;
        guid_index_size_ = (heap_sizes & 0x04) != 0 ? 4U : 2U;

        const auto str_idx = string_index_size_;
        const auto blob_idx = blob_index_size_;
        const auto guid_idx = guid_index_size_;

        const auto simple = [&](uint32_t table) { return simple_index_size(row_counts_[table]); };
        const auto coded = [&](std::initializer_list<int> tables, uint32_t tag_bits) {
            return coded_index_size(max_row_count(tables), tag_bits);
        };

        table_row_sizes_.assign(64, 0);
        table_offsets_.assign(64, 0);

        auto set_row_size = [&](int table, uint32_t size) { table_row_sizes_[table] = size; };

        set_row_size(0, 2 + str_idx + guid_idx * 3);
        set_row_size(1, coded({0, 26, 35, 1}, 2) + str_idx + str_idx);
        set_row_size(2, 4 + str_idx + str_idx + coded({2, 1, 27}, 2) + simple(4) + simple(6));
        set_row_size(3, simple(4));
        set_row_size(4, 2 + str_idx + blob_idx);
        set_row_size(5, simple(6));
        set_row_size(6, 4 + 2 + 2 + str_idx + blob_idx + simple(8));
        set_row_size(7, simple(8));
        set_row_size(8, 2 + 2 + str_idx);
        set_row_size(9, simple(2) + coded({2, 1, 27}, 2));
        set_row_size(10, coded({2, 1, 26, 6, 27}, 3) + str_idx + blob_idx);
        set_row_size(11, 2 + coded({4, 8, 23}, 2) + blob_idx);
        set_row_size(12, coded({6, 4, 1, 2, 8, 9, 10, 0, 14, 23, 20, 17, 26, 27, 32, 35, 38, 39, 40}, 5) +
                            coded({1, 2, 6, 10}, 2) + blob_idx);
        set_row_size(13, coded({4, 8}, 1) + blob_idx);
        set_row_size(14, 2 + coded({2, 6, 32}, 2) + blob_idx);
        set_row_size(15, 2 + 4 + simple(2));
        set_row_size(16, 4 + simple(4));
        set_row_size(17, blob_idx);
        set_row_size(18, simple(2) + simple(20));
        set_row_size(19, simple(20));
        set_row_size(20, 2 + str_idx + coded({2, 1, 27}, 2));
        set_row_size(21, simple(2) + simple(23));
        set_row_size(22, simple(23));
        set_row_size(23, 2 + str_idx + blob_idx);
        set_row_size(24, 2 + simple(6) + coded({20, 23}, 1));
        set_row_size(25, simple(2) + coded({6, 10}, 1) + coded({6, 10}, 1));
        set_row_size(26, str_idx);
        set_row_size(27, blob_idx);
        set_row_size(28, 2 + coded({4, 6}, 1) + str_idx + simple(26));
        set_row_size(29, 4 + simple(4));
        set_row_size(30, 4 + 4);
        set_row_size(31, 4);
        set_row_size(32, 4 + 2 + 2 + 2 + 2 + 4 + blob_idx + str_idx + str_idx);
        set_row_size(33, 4);
        set_row_size(34, 4 + 4 + 4);
        set_row_size(35, 2 + 2 + 2 + 2 + 4 + blob_idx + str_idx + str_idx + blob_idx);
        set_row_size(36, 4 + simple(35));
        set_row_size(37, 4 + 4 + 4 + simple(35));
        set_row_size(38, 4 + str_idx + blob_idx);
        set_row_size(39, 4 + 4 + str_idx + str_idx + coded({38, 35, 39}, 2));
        set_row_size(40, 4 + 4 + str_idx + coded({38, 35, 39}, 2));
        set_row_size(41, simple(2) + simple(2));
        set_row_size(42, 2 + 2 + str_idx + coded({2, 6}, 1));
        set_row_size(43, coded({6, 10}, 1) + blob_idx);
        set_row_size(44, simple(42) + coded({2, 1, 27}, 2));
        set_row_size(48, blob_idx + guid_idx + blob_idx + guid_idx);
        set_row_size(49, simple(48) + blob_idx);
        set_row_size(50, simple(6) + simple(53) + simple(51) + simple(52) + 4 + 4);
        set_row_size(51, 2 + 2 + str_idx);
        set_row_size(52, str_idx + blob_idx);
        set_row_size(53, simple(53) + blob_idx);
        set_row_size(54, simple(6) + simple(6));
        set_row_size(55, coded({6, 48, 51, 52, 53, 54}, 3) + guid_idx + blob_idx);

        const size_t available_table_bytes = tables_data_.size() - tables_start_;
        size_t offset = 0;
        for (int i = 0; i < 64; ++i) {
            if (((tables_valid_ >> i) & 1ULL) != 0) {
                if (row_counts_[i] > kMaxMetadataTableRows) {
                    return false;
                }
                if (row_counts_[i] > 0 && table_row_sizes_[i] == 0) {
                    return false;
                }
                table_offsets_[i] = static_cast<uint32_t>(offset);
                const size_t table_bytes =
                    static_cast<size_t>(table_row_sizes_[i]) * static_cast<size_t>(row_counts_[i]);
                if (table_bytes > available_table_bytes - offset) {
                    return false;
                }
                offset += table_bytes;
            }
        }

        if (offset > available_table_bytes) {
            return false;
        }

        valid_tables_.assign(tables_data_.begin() + tables_start_,
                             tables_data_.begin() + tables_start_ + offset);

        for (int i = 0; i < 64; ++i) {
            if (((tables_valid_ >> i) & 1ULL) != 0 && row_counts_[i] > 0) {
                read_table_rows(static_cast<TableId>(i), row_counts_[i]);
            }
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void CliMetadata::parse_metadata_root() {
    auto metadata_bytes = pe_.read_at_rva(pe_.cli_header().metadata_rva, pe_.cli_header().metadata_size);
    BinaryReader root(metadata_bytes);
    if (root.read_u32() != 0x424A5342) {
        throw std::runtime_error("invalid metadata signature (expected BSJB)");
    }
    root.skip(8);  // major, minor, reserved
    const uint32_t version_length = root.read_u32();
    root.skip(version_length);
    while ((root.position() % 4) != 0 && !root.eof()) {
        root.read_u8();
    }
    root.skip(2);  // flags
    const uint16_t stream_count = root.read_u16();

    struct StreamInfo {
        uint32_t offset;
        uint32_t size;
        std::string name;
    };
    std::vector<StreamInfo> streams;
    streams.reserve(stream_count);
    for (uint16_t i = 0; i < stream_count; ++i) {
        StreamInfo info{};
        info.offset = root.read_u32();
        info.size = root.read_u32();
        info.name = root.read_cstring();
        while ((root.position() % 4) != 0 && !root.eof()) {
            root.read_u8();
        }
        streams.push_back(info);
    }

    auto load_stream = [&](const std::string& expected, std::vector<uint8_t>& target) {
        uint32_t best_size = 0;
        uint32_t best_offset = 0;
        for (const auto& stream : streams) {
            if (stream.name != expected) {
                continue;
            }
            if (stream.size > best_size) {
                best_size = stream.size;
                best_offset = stream.offset;
            }
        }
        if (best_size > 0) {
            target.assign(metadata_bytes.begin() + best_offset,
                          metadata_bytes.begin() + best_offset + best_size);
        }
    };

    load_stream("#Strings", strings_heap_);
    load_stream("#Blob", blob_heap_);
    load_stream("#GUID", guid_heap_);
    load_stream("#US", us_heap_);
    load_stream("#~", tables_data_);
    if (tables_data_.empty()) {
        load_stream("#-", tables_data_);
    }
    if (tables_data_.empty()) {
        throw std::runtime_error("metadata stream #~ not found");
    }
    if (strings_heap_.empty() || blob_heap_.empty()) {
        throw std::runtime_error("required metadata heaps are missing");
    }
}

uint32_t CliMetadata::max_row_count(std::initializer_list<int> tables) const {
    uint32_t max_rows = 0;
    for (int table : tables) {
        max_rows = std::max(max_rows, row_counts_[table]);
    }
    return max_rows;
}

uint32_t CliMetadata::simple_index_size(uint32_t max_rows) const {
    return max_rows > 65535 ? 4U : 2U;
}

uint32_t CliMetadata::coded_index_size(uint32_t max_rows, uint32_t tag_bits) const {
    const uint32_t threshold = 1U << (16 - tag_bits);
    return max_rows >= threshold ? 4U : 2U;
}

uint32_t CliMetadata::decode_coded_index(uint32_t coded, uint32_t tag_bits) const {
    return coded >> tag_bits;
}

void CliMetadata::read_table_rows(TableId id, size_t row_count) {
    const int table_index = static_cast<int>(id);
    const uint32_t row_size = table_row_sizes_[table_index];
    const size_t base = table_offsets_[table_index];

    if (id == TableId::TypeDef) {
        if (obfuscated_metadata_) {
            read_typedef_rows_with_skew(row_count, row_size, base);
        } else {
            read_typedef_rows_standard(row_count, row_size, base);
        }
        return;
    }
    if (id == TableId::MethodDef) {
        if (obfuscated_metadata_) {
            read_methoddef_rows_with_skew(row_count, row_size, base);
        } else {
            read_methoddef_rows_standard(row_count, row_size, base);
        }
        return;
    }

    const auto str_idx = string_index_size_;
    const auto blob_idx = blob_index_size_;

    const auto read_index = [&](const uint8_t* row, size_t& pos, uint32_t size) -> uint32_t {
        uint32_t value = 0;
        if (size == 2) {
            std::memcpy(&value, row + pos, 2);
        } else {
            std::memcpy(&value, row + pos, 4);
        }
        pos += size;
        return value;
    };

    for (size_t row = 0; row < row_count; ++row) {
        const size_t row_offset = base + row * row_size;
        if (!safe_row_range(row_offset, row_size, valid_tables_.size())) {
            throw std::runtime_error("metadata row read out of bounds in table 0x" +
                                     std::to_string(table_index));
        }
        const uint8_t* row_data = valid_tables_.data() + row_offset;
        size_t pos = 0;

        switch (id) {
            case TableId::FieldPtr: {
                const uint32_t field_index =
                    read_index(row_data, pos, simple_index_size(row_counts_[4]));
                field_ptrs_.push_back(field_index);
                break;
            }
            case TableId::MethodPtr: {
                const uint32_t method_index =
                    read_index(row_data, pos, simple_index_size(row_counts_[6]));
                method_ptrs_.push_back(method_index);
                break;
            }
            case TableId::ParamPtr: {
                const uint32_t param_index =
                    read_index(row_data, pos, simple_index_size(row_counts_[8]));
                param_ptrs_.push_back(param_index);
                break;
            }
            case TableId::TypeDef:
                break;
            case TableId::Field: {
                FieldRow entry{};
                entry.flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
                entry.name_index = read_index(row_data, pos, str_idx);
                entry.signature_index = read_index(row_data, pos, blob_idx);
                fields_.push_back(entry);
                break;
            }
            case TableId::MethodDef:
                break;
            case TableId::Param: {
                ParamRow entry{};
                entry.flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
                entry.sequence = static_cast<uint16_t>(read_index(row_data, pos, 2));
                entry.name_index = read_index(row_data, pos, str_idx);
                params_.push_back(entry);
                break;
            }
            case TableId::MemberRef: {
                MemberRefRow entry{};
                entry.class_index = read_index(
                    row_data, pos, coded_index_size(max_row_count({2, 1, 26, 6, 27}), 3));
                entry.name_index = read_index(row_data, pos, str_idx);
                entry.signature_index = read_index(row_data, pos, blob_idx);
                member_refs_.push_back(entry);
                break;
            }
            case TableId::TypeRef: {
                TypeRefRow entry{};
                entry.resolution_scope_index = read_index(
                    row_data, pos, coded_index_size(max_row_count({0, 26, 35, 1}), 2));
                entry.type_name_index = read_index(row_data, pos, str_idx);
                entry.type_namespace_index = read_index(row_data, pos, str_idx);
                type_refs_.push_back(entry);
                break;
            }
            case TableId::TypeSpec: {
                TypeSpecRow entry{};
                entry.signature_index = read_index(row_data, pos, blob_idx);
                type_specs_.push_back(entry);
                break;
            }
            case TableId::NestedClass: {
                NestedClassRow entry{};
                entry.nested_class_index = read_index(row_data, pos, simple_index_size(row_counts_[2]));
                entry.enclosing_class_index = read_index(row_data, pos, simple_index_size(row_counts_[2]));
                nested_classes_.push_back(entry);
                break;
            }
            case TableId::Property: {
                PropertyRow entry{};
                entry.flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
                entry.name_index = read_index(row_data, pos, str_idx);
                entry.type_index = read_index(row_data, pos, blob_idx);
                properties_.push_back(entry);
                break;
            }
            case TableId::PropertyMap: {
                PropertyMapRow entry{};
                entry.parent_index = read_index(row_data, pos, simple_index_size(row_counts_[2]));
                entry.property_list_index = read_index(row_data, pos, simple_index_size(row_counts_[23]));
                property_maps_.push_back(entry);
                break;
            }
            default:
                break;
        }
    }
}

std::string CliMetadata::get_string(uint32_t index) const {
    if (index == 0 || index >= strings_heap_.size()) {
        return {};
    }
    const char* start = reinterpret_cast<const char*>(strings_heap_.data() + index);
    const char* heap_end = reinterpret_cast<const char*>(strings_heap_.data() + strings_heap_.size());
    const size_t max_len = static_cast<size_t>(heap_end - start);
    const void* null_ptr = std::memchr(start, '\0', max_len);
    if (null_ptr == nullptr) {
        return std::string(start, max_len);
    }
    return std::string(start, static_cast<const char*>(null_ptr) - start);
}

std::vector<uint8_t> CliMetadata::get_blob(uint32_t index) const {
    if (index == 0 || index >= blob_heap_.size()) {
        return {};
    }
    try {
        size_t offset = index;
        const uint32_t length = read_compressed_uint(blob_heap_.data(), blob_heap_.size(), offset);
        if (length == 0 || offset + length > blob_heap_.size()) {
            return {};
        }
        return std::vector<uint8_t>(blob_heap_.begin() + offset, blob_heap_.begin() + offset + length);
    } catch (const std::exception&) {
        return {};
    }
}

std::string CliMetadata::get_user_string(uint32_t offset) const {
    if (offset >= us_heap_.size()) {
        return {};
    }
    size_t pos = offset;
    const uint32_t byte_length = read_compressed_uint(us_heap_.data(), us_heap_.size(), pos);
    std::u16string utf16;
    utf16.reserve(byte_length / 2);
    for (uint32_t i = 0; i + 1 < byte_length; i += 2) {
        if (pos + 2 > us_heap_.size()) {
            break;
        }
        uint16_t ch{};
        std::memcpy(&ch, us_heap_.data() + pos, 2);
        pos += 2;
        utf16.push_back(ch);
    }
    std::string result;
    result.reserve(utf16.size());
    for (char16_t ch : utf16) {
        if (ch == 0) {
            break;
        }
        if (ch < 0x80) {
            result.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
            result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return result;
}

std::string CliMetadata::decode_type_signature(const uint8_t* data, size_t size,
                                               size_t& offset) const {
    std::unordered_set<uint32_t> typespec_visit;
    return decode_type_signature(data, size, offset, 0, typespec_visit);
}

std::string CliMetadata::decode_type_signature(const uint8_t* data, size_t size, size_t& offset,
                                               int depth,
                                               std::unordered_set<uint32_t>& typespec_visit) const {
    if (depth > kMaxTypeSignatureDepth) {
        return "...";
    }
    if (offset >= size) {
        return "void";
    }
    const uint8_t etype = data[offset++];
    switch (etype) {
        case 0x01:
            return "void";
        case 0x02:
            return "bool";
        case 0x03:
            return "char";
        case 0x04:
            return "sbyte";
        case 0x05:
            return "byte";
        case 0x06:
            return "short";
        case 0x07:
            return "ushort";
        case 0x08:
            return "int";
        case 0x09:
            return "uint";
        case 0x0A:
            return "long";
        case 0x0B:
            return "ulong";
        case 0x0C:
            return "float";
        case 0x0D:
            return "double";
        case 0x0E:
            return "string";
        case 0x11:
        case 0x12: {
            const uint32_t encoded = read_compressed_uint(data, size, offset);
            return resolve_type_encoded(encoded, depth + 1, typespec_visit);
        }
        case 0x13: {
            const uint32_t index = read_compressed_uint(data, size, offset);
            return "T" + std::to_string(index);
        }
        case 0x1D: {
            std::string element = decode_type_signature(data, size, offset, depth + 1, typespec_visit);
            return element + "[]";
        }
        case 0x15: {
            std::string base_type = decode_type_signature(data, size, offset, depth + 1, typespec_visit);
            const uint32_t argc = read_compressed_uint(data, size, offset);
            base_type += "<";
            for (uint32_t i = 0; i < argc; ++i) {
                if (i > 0) {
                    base_type += ", ";
                }
                base_type += decode_type_signature(data, size, offset, depth + 1, typespec_visit);
            }
            base_type += ">";
            return base_type;
        }
        case 0x1E: {
            const uint32_t index = read_compressed_uint(data, size, offset);
            return "TMethod" + std::to_string(index);
        }
        case 0x1C:
            return "object";
        case 0x18:
            return "nint";
        case 0x19:
            return "nuint";
        default:
            return "unknown";
    }
}

std::string CliMetadata::decode_type_name_from_blob(const uint8_t* data, size_t size,
                                                    size_t& offset) const {
    if (offset >= size) {
        return "Unknown";
    }
    std::string name;
    while (offset < size) {
        const uint8_t ch = data[offset++];
        if (ch == 0) {
            break;
        }
        if (ch <= 0x7F) {
            name.push_back(static_cast<char>(ch));
        } else if (ch <= 0xBF) {
            // UTF-8 continuation - simplified handling
            name.push_back('?');
        } else {
            name.push_back(static_cast<char>(ch));
        }
    }
    std::string nested;
    while (offset < size && data[offset] != 0) {
        if (!nested.empty()) {
            nested += ".";
        }
        while (offset < size) {
            const uint8_t ch = data[offset++];
            if (ch == 0) {
                break;
            }
            nested.push_back(static_cast<char>(ch));
        }
    }
    if (!nested.empty()) {
        name += "+" + nested;
    }
    return name.empty() ? "Unknown" : name;
}

MethodSignature CliMetadata::decode_method_signature(uint32_t blob_index) const {
    MethodSignature signature;
    signature.return_type = "void";
    try {
        const auto blob = get_blob(blob_index);
        if (blob.empty()) {
            return signature;
        }

        size_t offset = 0;
        const uint8_t calling = blob[offset++];
        signature.default_calling_convention = (calling & 0x0F) == 0x00;
        signature.has_this = (calling & 0x20) != 0;
        signature.explicit_this = (calling & 0x40) != 0;
        signature.param_count =
            std::min(read_compressed_uint(blob.data(), blob.size(), offset), 512U);
        signature.return_type = decode_type_signature(blob.data(), blob.size(), offset);
        for (uint32_t i = 0; i < signature.param_count; ++i) {
            signature.param_types.push_back(decode_type_signature(blob.data(), blob.size(), offset));
        }
    } catch (const std::exception&) {
        signature.param_types.clear();
        signature.return_type = "object";
    }
    return signature;
}

FieldSignature CliMetadata::decode_field_signature(uint32_t blob_index) const {
    FieldSignature signature;
    signature.type_name = "object";
    try {
        const auto blob = get_blob(blob_index);
        if (blob.size() < 2) {
            return signature;
        }
        size_t offset = 1;
        signature.type_name = decode_type_signature(blob.data(), blob.size(), offset);
    } catch (const std::exception&) {
        signature.type_name = "object";
    }
    return signature;
}

std::string CliMetadata::resolve_type_encoded(uint32_t encoded) const {
    std::unordered_set<uint32_t> typespec_visit;
    return resolve_type_encoded(encoded, 0, typespec_visit);
}

std::string CliMetadata::resolve_type_encoded(uint32_t encoded, int depth,
                                               std::unordered_set<uint32_t>& typespec_visit) const {
    if (depth > kMaxTypeSignatureDepth) {
        return "...";
    }
    const uint32_t tag = encoded & 0x3;
    const uint32_t index = encoded >> 2;
    if (index == 0) {
        return "object";
    }

    if (tag == 0) {
        if (index > type_defs_.size()) {
            return "TypeDef?" + std::to_string(index);
        }
        const auto& type = type_defs_[index - 1];
        const std::string ns = get_string(type.type_namespace_index);
        const std::string name = get_string(type.type_name_index);
        return ns.empty() ? name : ns + "." + name;
    }
    if (tag == 1) {
        if (index > type_refs_.size()) {
            return "TypeRef?" + std::to_string(index);
        }
        const auto& type = type_refs_[index - 1];
        const std::string ns = get_string(type.type_namespace_index);
        const std::string name = get_string(type.type_name_index);
        return ns.empty() ? name : ns + "." + name;
    }
    if (tag == 2) {
        if (index == 0 || index > type_specs_.size()) {
            return "TypeSpec?" + std::to_string(index);
        }
        if (typespec_visit.count(index) != 0) {
            return "TypeSpec/*cycle*/";
        }
        typespec_visit.insert(index);
        const auto blob = get_blob(type_specs_[index - 1].signature_index);
        if (blob.empty()) {
            typespec_visit.erase(index);
            return "TypeSpec?" + std::to_string(index);
        }
        size_t inner = 0;
        const std::string resolved =
            decode_type_signature(blob.data(), blob.size(), inner, depth + 1, typespec_visit);
        typespec_visit.erase(index);
        return resolved;
    }
    return "object";
}

std::string CliMetadata::resolve_type_name(uint32_t coded_index) const {
    return resolve_type_encoded(coded_index);
}

std::string CliMetadata::resolve_type_token(uint32_t token) const {
    const uint32_t table = (token >> 24) & 0xFF;
    const uint32_t index = token & 0x00FFFFFF;
    if (table == 0x02) {
        if (index == 0 || index > type_defs_.size()) {
            return "TypeDef?" + std::to_string(index);
        }
        const auto& type = type_defs_[index - 1];
        const std::string ns = get_string(type.type_namespace_index);
        const std::string name = get_string(type.type_name_index);
        return ns.empty() ? name : ns + "." + name;
    }
    if (table == 0x01) {
        if (index == 0 || index > type_refs_.size()) {
            return "TypeRef?" + std::to_string(index);
        }
        const auto& type = type_refs_[index - 1];
        const std::string ns = get_string(type.type_namespace_index);
        const std::string name = get_string(type.type_name_index);
        return ns.empty() ? name : ns + "." + name;
    }
    if (table == 0x1B) {
        if (index == 0 || index > type_specs_.size()) {
            return "TypeSpec?" + std::to_string(index);
        }
        return resolve_type_encoded(static_cast<uint32_t>((index << 2) | 2));
    }
    return "type_token_0x" + std::to_string(token);
}

std::string CliMetadata::resolve_method_token(uint32_t token) const {
    const uint32_t table = (token >> 24) & 0xFF;
    const uint32_t index = token & 0x00FFFFFF;
    if (table == 0x0A) {
        return resolve_member_ref(index);
    }
    if (table == 0x06) {
        if (index > 0 && index <= method_defs_.size()) {
            return get_string(method_defs_[index - 1].name_index);
        }
    }
    if (table == 0x02) {
        return resolve_type_token(token) + "..ctor";
    }
    return "method_token_0x" + std::to_string(token);
}

std::string CliMetadata::resolve_member_ref(uint32_t index) const {
    if (index == 0 || index > member_refs_.size()) {
        return "member?" + std::to_string(index);
    }
    const auto& member = member_refs_[index - 1];
    const std::string name = get_string(member.name_index);
    const uint32_t tag = member.class_index & 0x7;
    const uint32_t parent_index = member.class_index >> 3;
    std::string declaring;
    switch (tag) {
        case 0:
            if (parent_index > 0 && parent_index <= type_defs_.size()) {
                const auto& type = type_defs_[parent_index - 1];
                const std::string ns = get_string(type.type_namespace_index);
                const std::string type_name = get_string(type.type_name_index);
                declaring = ns.empty() ? type_name : ns + "." + type_name;
            }
            break;
        case 1:
            if (parent_index > 0 && parent_index <= type_refs_.size()) {
                const auto& type = type_refs_[parent_index - 1];
                const std::string ns = get_string(type.type_namespace_index);
                const std::string type_name = get_string(type.type_name_index);
                declaring = ns.empty() ? type_name : ns + "." + type_name;
            }
            break;
        case 3:
            if (parent_index > 0 && parent_index <= method_defs_.size()) {
                declaring = get_string(method_defs_[parent_index - 1].name_index);
            }
            break;
        default:
            declaring = "parent?" + std::to_string(parent_index);
            break;
    }
    return declaring.empty() ? name : declaring + "." + name;
}

std::vector<uint8_t> CliMetadata::get_method_il(uint32_t method_rva) const {
    if (method_rva == 0) {
        return {};
    }

    const size_t max_size = pe_.max_read_size_at_rva(method_rva);
    const size_t peek_size = std::min(max_size, static_cast<size_t>(12));
    const auto header_bytes = pe_.read_at_rva(method_rva, peek_size);
    const MethodHeader header =
        parse_method_header(header_bytes, max_size);
    return pe_.read_at_rva(method_rva, method_body_total_size(header));
}

std::vector<size_t> CliMetadata::fields_for_type(size_t type_def_index) const {
    std::vector<size_t> result;
    if (type_def_index >= type_defs_.size()) {
        return result;
    }
    const uint32_t start = resolve_field_rid(type_defs_[type_def_index].field_list_index);
    const uint32_t end = resolve_field_rid(
        (type_def_index + 1 < type_defs_.size()) ? type_defs_[type_def_index + 1].field_list_index
                                                 : static_cast<uint32_t>(fields_.size() + 1));
    append_rid_list(start, end, static_cast<uint32_t>(fields_.size()), result);
    return result;
}

std::vector<size_t> CliMetadata::methods_for_type(size_t type_def_index) const {
    std::vector<size_t> result;
    if (type_def_index >= type_defs_.size()) {
        return result;
    }
    const uint32_t start = resolve_method_rid(type_defs_[type_def_index].method_list_index);
    const uint32_t end = resolve_method_rid(
        (type_def_index + 1 < type_defs_.size()) ? type_defs_[type_def_index + 1].method_list_index
                                                 : static_cast<uint32_t>(method_defs_.size() + 1));
    append_rid_list(start, end, static_cast<uint32_t>(method_defs_.size()), result);
    return result;
}

std::vector<size_t> CliMetadata::params_for_method(size_t method_def_index) const {
    std::vector<size_t> result;
    if (method_def_index >= method_defs_.size()) {
        return result;
    }
    const uint32_t start = resolve_param_rid(method_defs_[method_def_index].param_list_index);
    const uint32_t end = resolve_param_rid(
        (method_def_index + 1 < method_defs_.size()) ? method_defs_[method_def_index + 1].param_list_index
                                                       : static_cast<uint32_t>(params_.size() + 1));
    append_rid_list(start, end, static_cast<uint32_t>(params_.size()), result);
    return result;
}

std::vector<size_t> CliMetadata::nested_types_for_type(size_t type_def_index) const {
    std::vector<size_t> result;
    const uint32_t target = static_cast<uint32_t>(type_def_index + 1);
    for (size_t i = 0; i < nested_classes_.size(); ++i) {
        if (nested_classes_[i].enclosing_class_index == target) {
            result.push_back(i);
        }
    }
    return result;
}

std::vector<size_t> CliMetadata::properties_for_type(size_t type_def_index) const {
    std::vector<size_t> result;
    if (!has_table(TableId::Property) || property_maps_.empty()) {
        return result;
    }

    const uint32_t target = static_cast<uint32_t>(type_def_index + 1);
    for (size_t map_index = 0; map_index < property_maps_.size(); ++map_index) {
        if (property_maps_[map_index].parent_index != target) {
            continue;
        }
        const uint32_t start = property_maps_[map_index].property_list_index;
        const uint32_t end = (map_index + 1 < property_maps_.size())
                                 ? property_maps_[map_index + 1].property_list_index
                                 : static_cast<uint32_t>(properties_.size() + 1);
        append_rid_list(start, end, static_cast<uint32_t>(properties_.size()), result);
        break;
    }
    return result;
}

}  // namespace csdecomp
