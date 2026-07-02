#include "csdecomp/cli_metadata.hpp"

#include "csdecomp/method_body.hpp"

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <sstream>
#include <stdexcept>

namespace csdecomp {

namespace {

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

}  // namespace

CliMetadata::CliMetadata(const PeReader& pe) : pe_(pe) {
    parse_metadata_root();
    parse_tables();
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
        for (const auto& stream : streams) {
            if (stream.name == expected) {
                target.assign(metadata_bytes.begin() + stream.offset,
                              metadata_bytes.begin() + stream.offset + stream.size);
                return;
            }
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

void CliMetadata::parse_tables() {
    BinaryReader reader(tables_data_);
    reader.skip(4);
    reader.skip(2);
    const uint8_t heap_sizes = reader.read_u8();
    const bool string_index_size = (heap_sizes & 0x01) != 0;
    const bool blob_index_size = (heap_sizes & 0x02) != 0;
    const bool guid_index_size = (heap_sizes & 0x04) != 0;
    string_index_size_ = string_index_size ? 4U : 2U;
    blob_index_size_ = blob_index_size ? 4U : 2U;
    guid_index_size_ = guid_index_size ? 4U : 2U;
    reader.skip(1);
    const uint64_t valid = reader.read_u64();
    reader.skip(8);

    row_counts_.assign(64, 0);
    for (int i = 0; i < 64; ++i) {
        if (((valid >> i) & 1ULL) != 0) {
            row_counts_[i] = reader.read_u32();
        }
    }

    const auto str_idx = string_index_size_;
    const auto blob_idx = blob_index_size_;
    const auto guid_idx = guid_index_size_;

    const auto simple = [&](uint32_t rows) { return simple_index_size(rows); };
    const auto coded = [&](uint32_t max_rows, uint32_t tag_bits) {
        return coded_index_size(max_rows, tag_bits);
    };

    table_row_sizes_.assign(64, 0);
    table_offsets_.assign(64, 0);

    const size_t tables_start = reader.position();

    auto set_row_size = [&](int table, uint32_t size) { table_row_sizes_[table] = size; };

    set_row_size(0, 2 + str_idx + guid_idx * 3);
    set_row_size(1, coded(max_row_count({0, 26, 35, 1}), 2) + str_idx + str_idx);
    set_row_size(2, 4 + str_idx + str_idx + coded(max_row_count({2, 1, 27}), 2) +
                        simple(row_counts_[4]) + simple(row_counts_[6]));
    set_row_size(4, 2 + str_idx + blob_idx);
    set_row_size(6, 4 + 2 + 2 + str_idx + blob_idx + simple(row_counts_[8]));
    set_row_size(8, 2 + 2 + str_idx);
    set_row_size(9, simple(row_counts_[2]) + coded(max_row_count({2, 1, 27}), 2));
    set_row_size(10, coded(max_row_count({2, 1, 26, 6, 27}), 3) + str_idx + blob_idx);
    set_row_size(11, 2 + coded(max_row_count({4, 8, 23}), 2) + blob_idx);
    set_row_size(12, coded(max_row_count({6, 4, 1, 2, 8, 9, 10, 0, 23, 20, 17, 26, 27, 32, 35, 38, 39, 40}),
                            5) +
                        coded(max_row_count({6, 10}), 1) + blob_idx);
    set_row_size(17, blob_idx);
    set_row_size(20, 2 + str_idx + coded(max_row_count({2, 1, 27}), 2));
    set_row_size(23, 2 + str_idx + blob_idx);
    set_row_size(26, str_idx);
    set_row_size(27, blob_idx);
    set_row_size(32, 4 + 2 + 2 + 2 + 2 + 4 + blob_idx + str_idx + str_idx);
    set_row_size(35, 2 + 2 + 2 + 2 + 4 + blob_idx + str_idx + str_idx + blob_idx);
    set_row_size(41, simple(row_counts_[2]) + simple(row_counts_[2]));

    size_t offset = 0;
    for (int i = 0; i < 64; ++i) {
        if (((valid >> i) & 1ULL) != 0) {
            table_offsets_[i] = static_cast<uint32_t>(offset);
            offset += static_cast<size_t>(table_row_sizes_[i]) * row_counts_[i];
        }
    }

    reader.seek(tables_start + offset);
    valid_tables_.assign(tables_data_.begin() + tables_start,
                         tables_data_.begin() + tables_start + offset);

    for (int i = 0; i < 64; ++i) {
        if (((valid >> i) & 1ULL) != 0 && row_counts_[i] > 0) {
            read_table_rows(static_cast<TableId>(i), row_counts_[i]);
        }
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
        const uint8_t* row_data = valid_tables_.data() + base + row * row_size;
        size_t pos = 0;

        switch (id) {
            case TableId::TypeDef: {
                TypeDefRow entry{};
                entry.flags = read_index(row_data, pos, 4);
                entry.type_name_index = read_index(row_data, pos, str_idx);
                entry.type_namespace_index = read_index(row_data, pos, str_idx);
                entry.extends_index = read_index(
                    row_data, pos, coded_index_size(max_row_count({2, 1, 27}), 2));
                entry.field_list_index = read_index(row_data, pos, simple_index_size(row_counts_[4]));
                entry.method_list_index = read_index(row_data, pos, simple_index_size(row_counts_[6]));
                type_defs_.push_back(entry);
                break;
            }
            case TableId::Field: {
                FieldRow entry{};
                entry.flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
                entry.name_index = read_index(row_data, pos, str_idx);
                entry.signature_index = read_index(row_data, pos, blob_idx);
                fields_.push_back(entry);
                break;
            }
            case TableId::MethodDef: {
                MethodDefRow entry{};
                entry.rva = read_index(row_data, pos, 4);
                entry.impl_flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
                entry.flags = static_cast<uint16_t>(read_index(row_data, pos, 2));
                entry.name_index = read_index(row_data, pos, str_idx);
                entry.signature_index = read_index(row_data, pos, blob_idx);
                entry.param_list_index = read_index(row_data, pos, simple_index_size(row_counts_[8]));
                method_defs_.push_back(entry);
                break;
            }
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
    return std::string(start);
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

std::string CliMetadata::decode_type_signature(const uint8_t* data, size_t size, size_t& offset) const {
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
            return resolve_type_encoded(encoded);
        }
        case 0x13: {
            const uint32_t index = read_compressed_uint(data, size, offset);
            return "T" + std::to_string(index);
        }
        case 0x1D: {
            std::string element = decode_type_signature(data, size, offset);
            return element + "[]";
        }
        case 0x15: {
            std::string base_type = decode_type_signature(data, size, offset);
            const uint32_t argc = read_compressed_uint(data, size, offset);
            base_type += "<";
            for (uint32_t i = 0; i < argc; ++i) {
                if (i > 0) {
                    base_type += ", ";
                }
                base_type += decode_type_signature(data, size, offset);
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
    const auto blob = get_blob(blob_index);
    if (blob.empty()) {
        signature.return_type = "void";
        return signature;
    }

    size_t offset = 0;
    const uint8_t calling = blob[offset++];
    signature.default_calling_convention = (calling & 0x0F) == 0x00;
    signature.has_this = (calling & 0x20) != 0;
    signature.explicit_this = (calling & 0x40) != 0;
    signature.param_count = read_compressed_uint(blob.data(), blob.size(), offset);
    signature.return_type = decode_type_signature(blob.data(), blob.size(), offset);
    for (uint32_t i = 0; i < signature.param_count; ++i) {
        signature.param_types.push_back(decode_type_signature(blob.data(), blob.size(), offset));
    }
    return signature;
}

FieldSignature CliMetadata::decode_field_signature(uint32_t blob_index) const {
    FieldSignature signature;
    const auto blob = get_blob(blob_index);
    if (blob.size() < 2) {
        signature.type_name = "object";
        return signature;
    }
    size_t offset = 1;
    signature.type_name = decode_type_signature(blob.data(), blob.size(), offset);
    return signature;
}

std::string CliMetadata::resolve_type_encoded(uint32_t encoded) const {
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
        const auto blob = get_blob(type_specs_[index - 1].signature_index);
        if (blob.empty()) {
            return "TypeSpec?" + std::to_string(index);
        }
        size_t inner = 0;
        return decode_type_signature(blob.data(), blob.size(), inner);
    }
    return "Type?" + std::to_string(encoded);
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
    const uint32_t start = type_defs_[type_def_index].field_list_index;
    const uint32_t end =
        (type_def_index + 1 < type_defs_.size()) ? type_defs_[type_def_index + 1].field_list_index
                                                 : static_cast<uint32_t>(fields_.size() + 1);
    if (start == 0) {
        return result;
    }
    for (uint32_t i = start; i < end; ++i) {
        result.push_back(i - 1);
    }
    return result;
}

std::vector<size_t> CliMetadata::methods_for_type(size_t type_def_index) const {
    std::vector<size_t> result;
    if (type_def_index >= type_defs_.size()) {
        return result;
    }
    const uint32_t start = type_defs_[type_def_index].method_list_index;
    const uint32_t end = (type_def_index + 1 < type_defs_.size())
                             ? type_defs_[type_def_index + 1].method_list_index
                             : static_cast<uint32_t>(method_defs_.size() + 1);
    if (start == 0) {
        return result;
    }
    for (uint32_t i = start; i < end; ++i) {
        result.push_back(i - 1);
    }
    return result;
}

std::vector<size_t> CliMetadata::params_for_method(size_t method_def_index) const {
    std::vector<size_t> result;
    if (method_def_index >= method_defs_.size()) {
        return result;
    }
    const uint32_t start = method_defs_[method_def_index].param_list_index;
    const uint32_t end = (method_def_index + 1 < method_defs_.size())
                             ? method_defs_[method_def_index + 1].param_list_index
                             : static_cast<uint32_t>(params_.size() + 1);
    if (start == 0) {
        return result;
    }
    for (uint32_t i = start; i < end; ++i) {
        result.push_back(i - 1);
    }
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

}  // namespace csdecomp
