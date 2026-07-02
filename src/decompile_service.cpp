#include "csdecomp/decompile_service.hpp"

#include "csdecomp/cli_metadata.hpp"
#include "csdecomp/decompiler.hpp"
#include "csdecomp/pe_reader.hpp"

#include <fstream>
#include <sstream>
#include <vector>

namespace csdecomp {

namespace {

std::vector<uint8_t> read_file_bytes(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

void decompile_loaded(const PeReader& pe, const CliMetadata& metadata, std::ostream& out,
                      const DecompileOptions& options) {
    Decompiler decompiler(pe, metadata);
    decompiler.decompile_assembly(out, options);
    out.flush();
}

}  // namespace

std::string decompile_to_string(const DecompileRequest& request) {
    if (request.input_path.empty()) {
        throw std::runtime_error("input file is not selected");
    }

    const auto bytes = read_file_bytes(request.input_path);
    PeReader pe(bytes);
    CliMetadata metadata(pe);

    std::ostringstream buffer;
    decompile_loaded(pe, metadata, buffer, request.options);
    return buffer.str();
}

void decompile_to_file(const DecompileRequest& request) {
    if (request.input_path.empty()) {
        throw std::runtime_error("input file is not selected");
    }

    const std::string output_path = request.output_path.empty()
                                        ? request.input_path + ".decompiled.cs"
                                        : request.output_path;

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open output file: " + output_path);
    }

    out.flush();

    try {
        const auto bytes = read_file_bytes(request.input_path);
        PeReader pe(bytes);
        CliMetadata metadata(pe);
        decompile_loaded(pe, metadata, out, request.options);
        out.flush();
    } catch (const std::exception& ex) {
        out << "\n// csdecomp: failed: " << ex.what() << "\n";
        out.flush();
        throw std::runtime_error(ex.what());
    } catch (...) {
        out << "\n// csdecomp: failed with unknown error\n";
        out.flush();
        throw std::runtime_error("unknown decompilation error");
    }
}

}  // namespace csdecomp
