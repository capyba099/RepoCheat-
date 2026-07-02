#pragma once

#include "csdecomp/cli_metadata.hpp"
#include "csdecomp/il_reader.hpp"

#include <ostream>
#include <string>

namespace csdecomp {

struct DecompileOptions {
    bool include_il_comments{false};
    bool emit_attributes{true};
};

class Decompiler {
public:
    Decompiler(const PeReader& pe, const CliMetadata& metadata);

    void decompile_assembly(std::ostream& out, const DecompileOptions& options = {}) const;
    void decompile_type(std::ostream& out, size_t type_def_index,
                        const DecompileOptions& options = {}) const;
    std::string decompile_method(size_t method_def_index, size_t type_def_index,
                                 const DecompileOptions& options = {}) const;

private:
    std::string decompile_method_body(size_t method_def_index, size_t type_def_index,
                                      const DecompileOptions& options,
                                      const std::vector<std::string>& param_names) const;
    std::string format_method_flags(uint16_t flags) const;
    std::string format_type_flags(uint32_t flags) const;
    std::string format_field_flags(uint16_t flags) const;
    std::string indent(int level) const;

    const PeReader& pe_;
    const CliMetadata& metadata_;
};

}  // namespace csdecomp
