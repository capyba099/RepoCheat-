#include "csdecomp/decompile_service.hpp"
#include "csdecomp/gui_win32.hpp"

#include <iostream>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cerr << "csdecomp - C# assembly decompiler (C++)\n\n"
              << "Usage:\n"
              << "  " << program << " [assembly.dll|exe] [options]\n\n"
              << "Options:\n"
              << "  -o, --output <file>   Write decompiled C# to file (default: stdout)\n"
              << "  --il-comments         Include IL offset comments in method bodies\n"
              << "  -h, --help            Show this help message\n"
#ifdef _WIN32
              << "\nRun without arguments to open the graphical interface.\n"
#endif
        ;
}

int run_cli(int argc, char** argv) {
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

    csdecomp::DecompileRequest request;
    request.input_path = std::move(input_path);
    request.output_path = std::move(output_path);
    request.options = options;

    if (request.output_path.empty()) {
        std::cout << csdecomp::decompile_to_string(request);
    } else {
        csdecomp::decompile_to_file(request);
        std::cerr << "Decompiled C# written to " << request.output_path << "\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        if (argc <= 1) {
            return csdecomp::run_win32_gui();
        }
#endif
        return run_cli(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
