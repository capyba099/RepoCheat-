#pragma once

#include "csdecomp/pe_reader.hpp"
#include "csdecomp/metadata_names.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace csdecomp {

enum class TableId : uint8_t {
    Module = 0,
    TypeRef = 1,
    TypeDef = 2,
    FieldPtr = 3,
    Field = 4,
    MethodPtr = 5,
    MethodDef = 6,
    ParamPtr = 7,
    Param = 8,
    InterfaceImpl = 9,
    MemberRef = 10,
    Constant = 11,
    CustomAttribute = 12,
    StandAloneSig = 17,
    Event = 20,
    Property = 23,
    MethodSemantics = 24,
    PropertyMap = 21,
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

struct PropertyRow {
    uint16_t flags{0};
    uint32_t name_index{0};
    uint32_t type_index{0};
};

struct PropertyMapRow {
    uint32_t parent_index{0};
    uint32_t property_list_index{0};
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
    [[nodiscard]] const std::vector<PropertyRow>& properties() const { return properties_; }

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
    [[nodiscard]] std::vector<size_t> properties_for_type(size_t type_def_index) const;

    [[nodiscard]] TypeDisplayName get_type_display_name(size_t type_def_index) const;
    [[nodiscard]] std::string get_method_display_name(uint32_t name_index, size_t method_index) const;
    [[nodiscard]] bool obfuscated_metadata() const { return obfuscated_metadata_; }

private:
    void parse_metadata_root();
    void read_tables_header();
    void clear_parsed_tables();
    bool parse_tables_with_heap_sizes(uint8_t heap_sizes);
    size_t score_metadata_layout() const;

    void read_table_rows(TableId id, size_t row_count);
    void read_typedef_rows_standard(size_t row_count, uint32_t row_size, size_t base);
    void read_methoddef_rows_standard(size_t row_count, uint32_t row_size, size_t base);
    void read_typedef_rows_with_skew(size_t row_count, uint32_t row_size, size_t base);
    void read_methoddef_rows_with_skew(size_t row_count, uint32_t row_size, size_t base);
    void read_memberref_rows_with_skew(size_t row_count, uint32_t row_size, size_t base);
    void read_memberref_rows_standard(size_t row_count, uint32_t row_size, size_t base);
    void read_field_rows_with_skew(size_t row_count, uint32_t row_size, size_t base);
    void read_field_rows_standard(size_t row_count, uint32_t row_size, size_t base);
    [[nodiscard]] size_t find_table_skew(size_t base, size_t row_count, uint32_t row_size,
                                         const std::function<int(const uint8_t*, uint32_t)>& score_row) const;
    [[nodiscard]] int score_typedef_row_bytes(const uint8_t* row, uint32_t row_size) const;
    [[nodiscard]] int score_methoddef_row_bytes(const uint8_t* row, uint32_t row_size) const;
    [[nodiscard]] int score_memberref_row_bytes(const uint8_t* row, uint32_t row_size) const;
    [[nodiscard]] int score_field_row_bytes(const uint8_t* row, uint32_t row_size) const;
    [[nodiscard]] bool has_table(TableId id) const;
    [[nodiscard]] uint32_t resolve_field_rid(uint32_t list_index) const;
    [[nodiscard]] uint32_t resolve_method_rid(uint32_t list_index) const;
    [[nodiscard]] uint32_t resolve_param_rid(uint32_t list_index) const;

    uint32_t coded_index_size(uint32_t max_rows, uint32_t tag_bits) const;
    uint32_t simple_index_size(uint32_t max_rows) const;
    uint32_t max_row_count(std::initializer_list<int> tables) const;
    uint32_t decode_coded_index(uint32_t coded, uint32_t tag_bits) const;

    std::string decode_type_signature(const uint8_t* data, size_t size, size_t& offset) const;
    std::string decode_type_signature(const uint8_t* data, size_t size, size_t& offset, int depth,
                                      std::unordered_set<uint32_t>& typespec_visit) const;
    std::string decode_type_name_from_blob(const uint8_t* data, size_t size, size_t& offset) const;
    std::string resolve_type_encoded(uint32_t encoded) const;
    std::string resolve_type_encoded(uint32_t encoded, int depth,
                                     std::unordered_set<uint32_t>& typespec_visit) const;

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
    uint64_t tables_valid_{0};
    size_t tables_start_{0};
    uint8_t tables_heap_sizes_declared_{0};
    bool obfuscated_metadata_{false};
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
    std::vector<uint32_t> field_ptrs_;
    std::vector<uint32_t> method_ptrs_;
    std::vector<uint32_t> param_ptrs_;
    std::vector<PropertyRow> properties_;
    std::vector<PropertyMapRow> property_maps_;
};

}  // namespace csdecomp
