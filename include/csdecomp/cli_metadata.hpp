#pragma once

#include "csdecomp/pe_reader.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace csdecomp {

enum class TableId : uint8_t {
    Module = 0,
    TypeRef = 1,
    TypeDef = 2,
    Field = 4,
    MethodDef = 6,
    Param = 8,
    InterfaceImpl = 9,
    MemberRef = 10,
    Constant = 11,
    CustomAttribute = 12,
    StandAloneSig = 17,
    Event = 20,
    Property = 23,
    ModuleRef = 26,
    TypeSpec = 27,
    Assembly = 32,
    AssemblyRef = 35,
    NestedClass = 41,
};

struct MethodDefRow {
    uint32_t rva{0};
    uint16_t impl_flags{0};
    uint16_t flags{0};
    uint32_t name_index{0};
    uint32_t signature_index{0};
    uint32_t param_list_index{0};
};

struct TypeDefRow {
    uint32_t flags{0};
    uint32_t type_name_index{0};
    uint32_t type_namespace_index{0};
    uint32_t extends_index{0};
    uint32_t field_list_index{0};
    uint32_t method_list_index{0};
};

struct FieldRow {
    uint16_t flags{0};
    uint32_t name_index{0};
    uint32_t signature_index{0};
};

struct ParamRow {
    uint16_t flags{0};
    uint16_t sequence{0};
    uint32_t name_index{0};
};

struct MemberRefRow {
    uint32_t class_index{0};
    uint32_t name_index{0};
    uint32_t signature_index{0};
};

struct TypeRefRow {
    uint32_t resolution_scope_index{0};
    uint32_t type_name_index{0};
    uint32_t type_namespace_index{0};
};

struct TypeSpecRow {
    uint32_t signature_index{0};
};

struct NestedClassRow {
    uint32_t nested_class_index{0};
    uint32_t enclosing_class_index{0};
};

struct MethodSignature {
    bool has_this{false};
    bool explicit_this{false};
    bool default_calling_convention{true};
    uint32_t param_count{0};
    std::string return_type;
    std::vector<std::string> param_types;
};

struct FieldSignature {
    std::string type_name;
};

class CliMetadata {
public:
    explicit CliMetadata(const PeReader& pe);

    [[nodiscard]] std::string get_string(uint32_t index) const;
    [[nodiscard]] std::vector<uint8_t> get_blob(uint32_t index) const;
    [[nodiscard]] std::string get_user_string(uint32_t offset) const;

    [[nodiscard]] const std::vector<MethodDefRow>& method_defs() const { return method_defs_; }
    [[nodiscard]] const std::vector<TypeDefRow>& type_defs() const { return type_defs_; }
    [[nodiscard]] const std::vector<FieldRow>& fields() const { return fields_; }
    [[nodiscard]] const std::vector<ParamRow>& params() const { return params_; }
    [[nodiscard]] const std::vector<MemberRefRow>& member_refs() const { return member_refs_; }
    [[nodiscard]] const std::vector<TypeRefRow>& type_refs() const { return type_refs_; }
    [[nodiscard]] const std::vector<TypeSpecRow>& type_specs() const { return type_specs_; }
    [[nodiscard]] const std::vector<NestedClassRow>& nested_classes() const { return nested_classes_; }

    [[nodiscard]] std::string resolve_type_name(uint32_t coded_index) const;
    [[nodiscard]] std::string resolve_member_ref(uint32_t index) const;
    [[nodiscard]] std::string resolve_method_token(uint32_t token) const;
    [[nodiscard]] std::string resolve_type_token(uint32_t token) const;
    [[nodiscard]] MethodSignature decode_method_signature(uint32_t blob_index) const;
    [[nodiscard]] FieldSignature decode_field_signature(uint32_t blob_index) const;
    [[nodiscard]] std::vector<uint8_t> get_method_il(uint32_t method_rva) const;

    [[nodiscard]] std::vector<size_t> fields_for_type(size_t type_def_index) const;
    [[nodiscard]] std::vector<size_t> methods_for_type(size_t type_def_index) const;
    [[nodiscard]] std::vector<size_t> params_for_method(size_t method_def_index) const;
    [[nodiscard]] std::vector<size_t> nested_types_for_type(size_t type_def_index) const;

private:
    void parse_metadata_root();
    void parse_tables();
    void read_table_rows(TableId id, size_t row_count);

    uint32_t coded_index_size(uint32_t max_rows, uint32_t tag_bits) const;
    uint32_t simple_index_size(uint32_t max_rows) const;
    uint32_t max_row_count(std::initializer_list<int> tables) const;
    uint32_t decode_coded_index(uint32_t coded, uint32_t tag_bits) const;

    std::string decode_type_signature(const uint8_t* data, size_t size, size_t& offset) const;
    std::string decode_type_name_from_blob(const uint8_t* data, size_t size, size_t& offset) const;
    std::string resolve_type_encoded(uint32_t encoded) const;

    const PeReader& pe_;
    std::vector<uint8_t> strings_heap_;
    std::vector<uint8_t> blob_heap_;
    std::vector<uint8_t> us_heap_;
    std::vector<uint8_t> guid_heap_;
    std::vector<uint8_t> tables_data_;
    std::vector<uint8_t> valid_tables_;
    std::vector<uint32_t> row_counts_;
    std::vector<uint32_t> table_row_sizes_;
    std::vector<uint32_t> table_offsets_;
    uint32_t string_index_size_{2};
    uint32_t blob_index_size_{2};
    uint32_t guid_index_size_{2};

    std::vector<MethodDefRow> method_defs_;
    std::vector<TypeDefRow> type_defs_;
    std::vector<FieldRow> fields_;
    std::vector<ParamRow> params_;
    std::vector<MemberRefRow> member_refs_;
    std::vector<TypeRefRow> type_refs_;
    std::vector<TypeSpecRow> type_specs_;
    std::vector<NestedClassRow> nested_classes_;
};

}  // namespace csdecomp
