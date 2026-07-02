#pragma once

#include "csdecomp/cli_metadata.hpp"
#include "csdecomp/il_reader.hpp"

#include <string>
#include <vector>

namespace csdecomp {

struct StructuredDecompileContext {
    const CliMetadata& metadata;
    size_t method_def_index{0};
    bool is_static{false};
    std::vector<std::string> param_names;
};

[[nodiscard]] std::string try_decompile_structured(const IlReader& il,
                                                   const StructuredDecompileContext& context);

[[nodiscard]] std::vector<std::string> restructure_goto_statements(
    std::vector<std::string> statements);

[[nodiscard]] std::vector<std::string> rename_local_variables(std::vector<std::string> statements);

}  // namespace csdecomp
