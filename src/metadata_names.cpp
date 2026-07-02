#include "csdecomp/metadata_names.hpp"

#include <algorithm>
#include <cctype>

namespace csdecomp {

namespace {

bool is_hex_token(const std::string& value) {
    if (value.size() < 6) {
        return false;
    }
    for (char ch : value) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

bool starts_with_ci(const std::string& value, const char* prefix) {
    const size_t len = std::char_traits<char>::length(prefix);
    if (value.size() < len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool is_obfuscated_namespace_token(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    if (starts_with_ci(value, "sya.") || starts_with_ci(value, "olesya.")) {
        const std::string tail = value.substr(value.find('.') + 1);
        return is_hex_token(tail);
    }
    if (starts_with_ci(value, "Olesya.")) {
        const std::string tail = value.substr(7);
        return is_hex_token(tail);
    }
    if (value.find('.') != std::string::npos) {
        const std::string leaf = value.substr(value.rfind('.') + 1);
        if (is_hex_token(leaf) && leaf.size() >= 6) {
            return true;
        }
    } else if (is_hex_token(value) && value.size() >= 6) {
        return true;
    }
    return false;
}

bool is_real_type_or_namespace_token(const std::string& value) {
    if (value.empty() || is_obfuscated_namespace_token(value)) {
        return false;
    }
    if (value[0] == '<' || value[0] == '.') {
        return true;
    }
    if (starts_with_ci(value, "System") || starts_with_ci(value, "Microsoft") ||
        starts_with_ci(value, "SharpMonoInjector") || starts_with_ci(value, "mscorlib")) {
        return true;
    }
    for (unsigned char ch : value) {
        if (ch < 0x20 || ch == 0x7F) {
            return false;
        }
    }
    return !value.empty();
}

bool is_namespace_only_typeref(const std::string& value) {
    if (value.empty() || value.find('.') == std::string::npos) {
        return value == "System";
    }
    static constexpr const char* kNamespaceLeaves[] = {
        "Security",         "Collections", "Reflection", "Threading", "IO",
        "Text",             "Diagnostics", "ComponentModel", "Runtime", "Resources",
        "Windows",          "Net",         "Globalization", "Configuration", "Management",
        "Linq",             "InteropServices", "CompilerServices", "CodeDom", "Principal",
        "Compression",      "Cryptography", "Data", "Input", "Controls", "Markup",
        "ObjectModel",      "Emit",        "Versioning", "Generic",
    };
    const size_t pos = value.rfind('.');
    const std::string leaf = value.substr(pos + 1);
    for (const char* candidate : kNamespaceLeaves) {
        if (leaf == candidate) {
            return true;
        }
    }
    return starts_with_ci(value, "System.") || starts_with_ci(value, "Microsoft.");
}

bool is_obfuscated_method_stub_name(const std::string& name) {
    if (name.rfind("Olesya.", 0) == 0) {
        return true;
    }
    if (name.rfind("set_Olesya.", 0) == 0 || name.rfind("get_Olesya.", 0) == 0) {
        return true;
    }
    static constexpr const char* kJunkMethodNames[] = {
        "Linq", "IO", "Xml", "Tasks", "Reflection", "Security", "Diagnostics",
        "Drawing", "Http", "ComponentModel", "Generic",
    };
    for (const char* junk : kJunkMethodNames) {
        if (name == junk) {
            return true;
        }
    }
    return false;
}

TypeDisplayName split_type_display_name(const std::string& namespace_name,
                                        const std::string& raw_name) {
    TypeDisplayName result;
    std::string ns = namespace_name;
    std::string name = raw_name;

    if (name.empty()) {
        result.namespace_name = ns;
        result.name = "Type";
        return result;
    }

    if (ns.empty() && name.find('.') != std::string::npos && name.rfind("Olesya.", 0) != 0 &&
        name.rfind("<>", 0) != 0 && name[0] != '<' && name[0] != '.') {
        const size_t split = name.rfind('.');
        ns = name.substr(0, split);
        name = name.substr(split + 1);
    }

    if (name.rfind("Olesya.", 0) == 0) {
        const std::string short_name = name.substr(7);
        if (ns.empty()) {
            ns = "Olesya";
        }
        name = short_name;
    }

    result.namespace_name = ns;
    result.name = name;
    return result;
}

std::string format_typeref_display_name(const std::string& type_name_field,
                                        const std::string& namespace_field,
                                        bool obfuscated_metadata) {
    if (!obfuscated_metadata) {
        if (namespace_field.empty()) {
            return type_name_field;
        }
        if (type_name_field.empty()) {
            return namespace_field;
        }
        return namespace_field + "." + type_name_field;
    }

    const bool name_junk = is_obfuscated_namespace_token(type_name_field);
    const bool ns_junk = is_obfuscated_namespace_token(namespace_field);
    const bool name_real = is_real_type_or_namespace_token(type_name_field);
    const bool ns_real = is_real_type_or_namespace_token(namespace_field);

    std::string qualified;
    if (name_junk && ns_real) {
        qualified = namespace_field;
    } else if (ns_junk && name_real) {
        qualified = type_name_field;
    } else if (!name_junk && !ns_junk) {
        if (namespace_field.empty()) {
            qualified = type_name_field;
        } else if (type_name_field.empty()) {
            qualified = namespace_field;
        } else {
            qualified = namespace_field + "." + type_name_field;
        }
    } else {
        return "object";
    }

    if (is_namespace_only_typeref(qualified)) {
        return "object";
    }
    return qualified;
}

std::string format_method_display_name(const std::string& raw_name, size_t method_index) {
    if (raw_name.empty()) {
        return "Method_" + std::to_string(method_index + 1);
    }
    if (raw_name == ".ctor" || raw_name == ".cctor") {
        return raw_name;
    }
    if (raw_name.rfind("Olesya.", 0) == 0) {
        return raw_name.substr(7);
    }
    if (raw_name.rfind("get_Olesya.", 0) == 0) {
        return "get_" + raw_name.substr(11);
    }
    if (raw_name.rfind("set_Olesya.", 0) == 0) {
        return "set_" + raw_name.substr(11);
    }
    if (raw_name[0] >= '0' && raw_name[0] <= '9') {
        return "@" + raw_name;
    }
    return raw_name;
}

std::string format_member_display_name(const std::string& raw_name) {
    if (raw_name.rfind("Olesya.", 0) == 0) {
        return raw_name.substr(7);
    }
    if (raw_name.rfind("sya.", 0) == 0 || raw_name.rfind("olesya.", 0) == 0) {
        return raw_name.substr(raw_name.find('.') + 1);
    }
    if (!raw_name.empty() && raw_name[0] == '.') {
        return raw_name.substr(1);
    }
    return raw_name;
}

bool is_junk_field_name(const std::string& name) {
    if (name.empty()) {
        return true;
    }
    if (name == "00" || name == "180" || name == "unknown") {
        return true;
    }
    if (name == "IO" || name == "Linq" || name == "Xml" || name == "Tasks" || name == "Reflection" ||
        name == "Security" || name == "Drawing" || name == "Http" || name == "Generic") {
        return true;
    }
    if (is_obfuscated_namespace_token(name)) {
        return true;
    }
    if (name.rfind("Olesya.", 0) == 0 || name.rfind("sya.", 0) == 0 || name.rfind("lesya.", 0) == 0) {
        return true;
    }
    if (is_hex_token(name) && name.size() >= 6) {
        return true;
    }
    if (name.size() <= 2 && std::all_of(name.begin(), name.end(), [](char ch) {
            return std::isxdigit(static_cast<unsigned char>(ch));
        })) {
        return true;
    }
    return false;
}

int type_decompile_priority(const TypeDisplayName& display) {
    if (display.namespace_name.find("SharpMonoInjector") != std::string::npos) {
        return 0;
    }
    if (display.namespace_name.find("Injector") != std::string::npos) {
        return 1;
    }
    if (display.namespace_name.rfind("Olesya", 0) == 0 ||
        (display.namespace_name.empty() && is_hex_token(display.name))) {
        return 3;
    }
    if (display.namespace_name == "Olesya") {
        return 3;
    }
    return 2;
}

}  // namespace csdecomp
