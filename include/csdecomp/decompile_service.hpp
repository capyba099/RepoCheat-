#pragma once

#include "csdecomp/decompiler.hpp"

#include <string>

namespace csdecomp {

struct DecompileRequest {
    std::string input_path;
    std::string output_path;
    DecompileOptions options{};
};

struct DecompileFileResult {
    bool ok{false};
    std::string error_message;
};

std::string decompile_to_string(const DecompileRequest& request);
DecompileFileResult decompile_to_file(const DecompileRequest& request);

}  // namespace csdecomp
