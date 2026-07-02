#pragma once

#include "csdecomp/decompiler.hpp"
#include "csdecomp/pe_reader.hpp"
#include "csdecomp/cli_metadata.hpp"

#include <string>
#include <vector>

namespace csdecomp {

struct ProjectEmitResult {
    bool ok{false};
    std::string error_message;
    std::string solution_path;
    std::string project_path;
    std::vector<std::string> source_files;
};

struct ProjectEmitOptions {
    std::string output_dir;
    std::string project_name;
    std::string target_framework{"net48"};
};

[[nodiscard]] std::string derive_project_name(const std::string& input_path);
[[nodiscard]] std::string sanitize_path_component(const std::string& value);
[[nodiscard]] std::string make_project_guid(const std::string& seed);

ProjectEmitResult emit_visual_studio_project(const PeReader& pe, const CliMetadata& metadata,
                                             const Decompiler& decompiler, const ProjectEmitOptions& options,
                                             const DecompileOptions& decompile_options);

}  // namespace csdecomp
