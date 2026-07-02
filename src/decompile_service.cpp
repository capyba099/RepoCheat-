#include "csdecomp/decompile_service.hpp"

#include "csdecomp/cli_metadata.hpp"
#include "csdecomp/decompiler.hpp"
#include "csdecomp/pe_reader.hpp"
#include "csdecomp/project_emitter.hpp"

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

DecompileFileResult decompile_to_file(const DecompileRequest& request) {
    DecompileFileResult result;

    if (request.input_path.empty()) {
        result.error_message = "input file is not selected";
        return result;
    }

    const std::string output_path = request.output_path.empty()
                                        ? request.input_path + ".decompiled.cs"
                                        : request.output_path;

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        result.error_message = "failed to open output file: " + output_path;
        return result;
    }

    try {
        out << "// csdecomp: version=";
#ifdef CSDECOMP_VERSION
        out << CSDECOMP_VERSION;
#else
        out << "unknown";
#endif
        out << "\n";
        out << "// csdecomp: loading assembly\n";
        out.flush();

        const auto bytes = read_file_bytes(request.input_path);
        out << "// csdecomp: parsing PE (" << bytes.size() << " bytes)\n";
        out.flush();

        PeReader pe(bytes);
        out << "// csdecomp: parsing metadata\n";
        out.flush();

        CliMetadata metadata(pe);
        out << "// csdecomp: metadata: types=" << metadata.type_defs().size()
            << " methods=" << metadata.method_defs().size()
            << " typerefs=" << metadata.type_refs().size() << "\n";
        out.flush();
        out << "// csdecomp: decompiling types\n";
        out.flush();

        decompile_loaded(pe, metadata, out, request.options);
        out.flush();
        result.ok = true;
        return result;
    } catch (const std::bad_alloc&) {
        result.error_message = "out of memory while decompiling";
    } catch (const std::exception& ex) {
        result.error_message = ex.what();
    } catch (...) {
        result.error_message = "unknown decompilation error";
    }

    out << "\n// csdecomp: failed: " << result.error_message << "\n";
    out.flush();
    return result;
}

DecompileFileResult decompile_to_project(const DecompileRequest& request) {
    DecompileFileResult result;

    if (request.input_path.empty()) {
        result.error_message = "input file is not selected";
        return result;
    }
    if (request.project_output_dir.empty()) {
        result.error_message = "project output directory is not selected";
        return result;
    }

    try {
        const auto bytes = read_file_bytes(request.input_path);
        PeReader pe(bytes);
        CliMetadata metadata(pe);
        Decompiler decompiler(pe, metadata);

        ProjectEmitOptions project_options;
        project_options.output_dir = request.project_output_dir;
        project_options.project_name = request.project_name.empty() ? derive_project_name(request.input_path)
                                                                    : request.project_name;
        project_options.target_framework = request.target_framework;

        const ProjectEmitResult emitted =
            emit_visual_studio_project(pe, metadata, decompiler, project_options, request.options);
        if (!emitted.ok) {
            result.error_message = emitted.error_message;
            return result;
        }

        result.ok = true;
        result.solution_path = emitted.solution_path;
        result.project_path = emitted.project_path;
        result.source_files = emitted.source_files;
        return result;
    } catch (const std::bad_alloc&) {
        result.error_message = "out of memory while decompiling";
    } catch (const std::exception& ex) {
        result.error_message = ex.what();
    } catch (...) {
        result.error_message = "unknown decompilation error";
    }
    return result;
}

}  // namespace csdecomp
