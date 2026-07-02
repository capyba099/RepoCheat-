#include "csdecomp/cli_metadata.hpp"
#include "csdecomp/decompiler.hpp"
#include "csdecomp/pe_reader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

void print_usage(const char* program) {
    std::cerr << "csdecomp - C# assembly decompiler (C++)\n\n"
              << "Usage:\n"
              << "  " << program << " <assembly.dll|exe> [options]\n\n"
              << "Options:\n"
              << "  -o, --output <file>   Write decompiled C# to file (default: stdout)\n"
              << "  --il-comments         Include IL offset comments in method bodies\n"
              << "  -h, --help            Show this help message\n";
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 1;
        }

        std::string input_path;
        std::string output_path;
        csdecomp::DecompileOptions options;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            }
            if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
                output_path = argv[++i];
                continue;
            }
            if (arg == "--il-comments") {
                options.include_il_comments = true;
                continue;
            }
            if (arg.rfind('-', 0) == 0) {
                std::cerr << "Unknown option: " << arg << "\n";
                return 1;
            }
            if (input_path.empty()) {
                input_path = arg;
            } else {
                std::cerr << "Unexpected argument: " << arg << "\n";
                return 1;
            }
        }

        if (input_path.empty()) {
            print_usage(argv[0]);
            return 1;
        }

        const auto bytes = read_file(input_path);
        csdecomp::PeReader pe(bytes);
        csdecomp::CliMetadata metadata(pe);
        csdecomp::Decompiler decompiler(pe, metadata);

        std::ostringstream buffer;
        decompiler.decompile_assembly(buffer, options);

        if (output_path.empty()) {
            std::cout << buffer.str();
        } else {
            std::ofstream out(output_path);
            if (!out) {
                throw std::runtime_error("failed to open output file: " + output_path);
            }
            out << buffer.str();
            std::cerr << "Decompiled C# written to " << output_path << "\n";
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
