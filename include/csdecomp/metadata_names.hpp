#pragma once

#include <string>

namespace csdecomp {

struct TypeDisplayName {
    std::string namespace_name;
    std::string name;
};

[[nodiscard]] bool is_obfuscated_namespace_token(const std::string& value);
[[nodiscard]] bool is_real_type_or_namespace_token(const std::string& value);
[[nodiscard]] bool is_namespace_only_typeref(const std::string& value);
[[nodiscard]] bool is_obfuscated_method_stub_name(const std::string& name);

[[nodiscard]] TypeDisplayName split_type_display_name(const std::string& namespace_name,
                                                      const std::string& raw_name);
[[nodiscard]] std::string format_typeref_display_name(const std::string& type_name_field,
                                                      const std::string& namespace_field,
                                                      bool obfuscated_metadata);
[[nodiscard]] std::string format_method_display_name(const std::string& raw_name, size_t method_index);
[[nodiscard]] std::string format_member_display_name(const std::string& raw_name);
[[nodiscard]] bool is_junk_field_name(const std::string& name);
[[nodiscard]] int type_decompile_priority(const TypeDisplayName& display);
[[nodiscard]] bool is_obfuscated_stub_type(const TypeDisplayName& display);

}  // namespace csdecomp
