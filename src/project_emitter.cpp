#include "csdecomp/project_emitter.hpp"

#include "csdecomp/metadata_names.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace csdecomp {

namespace {

constexpr uint32_t kFnvOffset = 2166136261U;
constexpr uint32_t kFnvPrime = 16777619U;

uint32_t fnv1a(const std::string& value) {
    uint32_t hash = kFnvOffset;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= kFnvPrime;
    }
    return hash;
}

std::string to_upper_hex(uint32_t value, size_t width) {
    std::ostringstream out;
    out.setf(std::ios::hex, std::ios::basefield);
    out.width(static_cast<std::streamsize>(width));
    out.fill('0');
    out << std::uppercase << value;
    return out.str();
}

bool write_text_file(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << content;
    return static_cast<bool>(out);
}

std::vector<size_t> root_type_indices(const CliMetadata& metadata) {
    std::vector<size_t> root_types;
    root_types.reserve(metadata.type_defs().size());
    for (size_t i = 0; i < metadata.type_defs().size(); ++i) {
        bool is_nested = false;
        for (const auto& nested : metadata.nested_classes()) {
            if (nested.nested_class_index == i + 1) {
                is_nested = true;
                break;
            }
        }
        if (!is_nested) {
            root_types.push_back(i);
        }
    }
    std::sort(root_types.begin(), root_types.end(),
              [&metadata](size_t left, size_t right) {
                  const int left_priority = type_decompile_priority(metadata.get_type_display_name(left));
                  const int right_priority = type_decompile_priority(metadata.get_type_display_name(right));
                  if (left_priority != right_priority) {
                      return left_priority < right_priority;
                  }
                  return left < right;
              });
    return root_types;
}

std::string unique_type_file_stem(const TypeDisplayName& display, size_t type_index,
                                  std::unordered_set<std::string>& used_names) {
    std::string stem = sanitize_path_component(display.name);
    if (stem.empty()) {
        stem = "Type_" + std::to_string(type_index + 1);
    }
    std::string candidate = stem;
    size_t suffix = 2;
    while (!used_names.insert(candidate).second) {
        candidate = stem + "_" + std::to_string(suffix++);
    }
    return candidate;
}

fs::path namespace_directory(const fs::path& project_dir, const std::string& namespace_name) {
    if (namespace_name.empty()) {
        return project_dir;
    }
    fs::path current = project_dir;
    size_t start = 0;
    while (start < namespace_name.size()) {
        size_t end = namespace_name.find('.', start);
        if (end == std::string::npos) {
            end = namespace_name.size();
        }
        const std::string segment = sanitize_path_component(namespace_name.substr(start, end - start));
        if (!segment.empty()) {
            current /= segment;
        }
        start = end + 1;
    }
    return current;
}

std::string render_csproj(const std::string& project_name, const std::string& target_framework) {
    std::ostringstream out;
    out << "<Project Sdk=\"Microsoft.NET.Sdk\">\n";
    out << "  <PropertyGroup>\n";
    out << "    <OutputType>Library</OutputType>\n";
    out << "    <TargetFramework>" << target_framework << "</TargetFramework>\n";
    out << "    <ImplicitUsings>disable</ImplicitUsings>\n";
    out << "    <Nullable>disable</Nullable>\n";
    out << "    <AssemblyName>" << project_name << "</AssemblyName>\n";
    out << "    <RootNamespace>" << project_name << "</RootNamespace>\n";
    out << "    <GenerateAssemblyInfo>false</GenerateAssemblyInfo>\n";
    out << "  </PropertyGroup>\n";
    out << "</Project>\n";
    return out.str();
}

std::string render_sln(const std::string& project_name, const std::string& project_guid) {
    std::ostringstream out;
    out << "Microsoft Visual Studio Solution File, Format Version 12.00\n";
    out << "# Visual Studio Version 17\n";
    out << "VisualStudioVersion = 17.0.31903.59\n";
    out << "MinimumVisualStudioVersion = 10.0.40219.1\n";
    out << "Project(\"{FAE04EC0-301F-11D3-BF4B-00C04F79EFBC}\") = \"" << project_name << "\", \""
        << project_name << "\\" << project_name << ".csproj\", \"{" << project_guid << "}\"\n";
    out << "EndProject\n";
    out << "Global\n";
    out << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n";
    out << "\t\tDebug|Any CPU = Debug|Any CPU\n";
    out << "\t\tRelease|Any CPU = Release|Any CPU\n";
    out << "\tEndGlobalSection\n";
    out << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";
    out << "\t\t{" << project_guid << "}.Debug|Any CPU.ActiveCfg = Debug|Any CPU\n";
    out << "\t\t{" << project_guid << "}.Debug|Any CPU.Build.0 = Debug|Any CPU\n";
    out << "\t\t{" << project_guid << "}.Release|Any CPU.ActiveCfg = Release|Any CPU\n";
    out << "\t\t{" << project_guid << "}.Release|Any CPU.Build.0 = Release|Any CPU\n";
    out << "\tEndGlobalSection\n";
    out << "EndGlobal\n";
    return out.str();
}

}  // namespace

std::string derive_project_name(const std::string& input_path) {
    const fs::path path(input_path);
    std::string stem = path.stem().string();
    if (stem.empty()) {
        return "DecompiledProject";
    }
    std::string sanitized = sanitize_path_component(stem);
    if (sanitized.empty()) {
        return "DecompiledProject";
    }
    if (std::isdigit(static_cast<unsigned char>(sanitized[0])) != 0) {
        sanitized = "Assembly_" + sanitized;
    }
    return sanitized;
}

std::string sanitize_path_component(const std::string& value) {
    std::string sanitized;
    sanitized.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '\\':
            case '/':
            case ':':
            case '*':
            case '?':
            case '"':
            case '<':
            case '>':
            case '|':
                sanitized.push_back('_');
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    sanitized.push_back('_');
                } else {
                    sanitized.push_back(ch);
                }
                break;
        }
    }
    while (!sanitized.empty() && sanitized.back() == ' ') {
        sanitized.pop_back();
    }
    while (!sanitized.empty() && sanitized.front() == ' ') {
        sanitized.erase(sanitized.begin());
    }
    return sanitized;
}

std::string make_project_guid(const std::string& seed) {
    const uint32_t h1 = fnv1a(seed);
    const uint32_t h2 = fnv1a(seed + "#2");
    const uint32_t h3 = fnv1a(seed + "#3");
    const uint32_t h4 = fnv1a(seed + "#4");
    return to_upper_hex(h1, 8) + "-" + to_upper_hex((h2 >> 16) & 0xFFFFU, 4) + "-" +
           to_upper_hex(h2 & 0xFFFFU, 4) + "-" + to_upper_hex((h3 >> 16) & 0xFFFFU, 4) + "-" +
           to_upper_hex(h3 & 0xFFFFU, 4) + to_upper_hex(h4, 8);
}

ProjectEmitResult emit_visual_studio_project(const PeReader& /*pe*/, const CliMetadata& metadata,
                                             const Decompiler& decompiler, const ProjectEmitOptions& options,
                                             const DecompileOptions& decompile_options) {
    ProjectEmitResult result;
    if (options.output_dir.empty()) {
        result.error_message = "project output directory is empty";
        return result;
    }

    const std::string project_name =
        options.project_name.empty() ? "DecompiledProject" : sanitize_path_component(options.project_name);
    const fs::path output_dir = fs::path(options.output_dir);
    const fs::path project_dir = output_dir / project_name;
    const fs::path csproj_path = project_dir / (project_name + ".csproj");
    const fs::path sln_path = output_dir / (project_name + ".sln");
    const std::string project_guid = make_project_guid(project_name);

    if (!write_text_file(csproj_path, render_csproj(project_name, options.target_framework))) {
        result.error_message = "failed to write project file: " + csproj_path.string();
        return result;
    }
    if (!write_text_file(sln_path, render_sln(project_name, project_guid))) {
        result.error_message = "failed to write solution file: " + sln_path.string();
        return result;
    }

    std::unordered_set<std::string> used_file_stems;
    const auto root_types = root_type_indices(metadata);
    for (const size_t type_index : root_types) {
        const TypeDisplayName display = metadata.get_type_display_name(type_index);
        if (is_obfuscated_stub_type(display)) {
            continue;
        }
        const std::string file_stem = unique_type_file_stem(display, type_index, used_file_stems);
        const fs::path type_dir = namespace_directory(project_dir, display.namespace_name);
        const fs::path source_path = type_dir / (file_stem + ".cs");

        std::ostringstream source;
        source << "// Decompiled with csdecomp\n";
        source << "// Type index: " << (type_index + 1) << "\n\n";
        source << "using System;\n\n";
        try {
            decompiler.decompile_type(source, type_index, decompile_options);
        } catch (const std::exception& ex) {
            source << "// Failed to decompile type #" << type_index << ": " << ex.what() << "\n";
        }

        if (!write_text_file(source_path, source.str())) {
            result.error_message = "failed to write source file: " + source_path.string();
            return result;
        }
        result.source_files.push_back(source_path.string());
    }

    result.ok = true;
    result.solution_path = sln_path.string();
    result.project_path = csproj_path.string();
    return result;
}

}  // namespace csdecomp
