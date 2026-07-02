#pragma once

#include "csdecomp/decompiler.hpp"

#include <string>
#include <vector>

namespace csdecomp {

struct DecompileRequest {
    std::string input_path;
    std::string output_path;
    std::string project_output_dir;
    std::string project_name;
    std::string target_framework{"net48"};
    DecompileOptions options{};
};

struct DecompileFileResult {
    bool ok{false};
    std::string error_message;
    std::string solution_path;
    std::string project_path;
    std::vector<std::string> source_files;
};

std::string decompile_to_string(const DecompileRequest& request);
DecompileFileResult decompile_to_file(const DecompileRequest& request);
DecompileFileResult decompile_to_project(const DecompileRequest& request);

}  // namespace csdecomp
