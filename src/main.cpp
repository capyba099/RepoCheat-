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
              << "  --project <dir>       Generate Visual Studio solution and project tree\n"
              << "  --project-name <name> Project/assembly name (default: input file stem)\n"
              << "  --target-framework <tfm>  Target framework for .csproj (default: net48)\n"
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
    std::string project_output_dir;
    std::string project_name;
    std::string target_framework = "net48";
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
        if (arg == "--project" && i + 1 < argc) {
            project_output_dir = argv[++i];
            continue;
        }
        if (arg == "--project-name" && i + 1 < argc) {
            project_name = argv[++i];
            continue;
        }
        if (arg == "--target-framework" && i + 1 < argc) {
            target_framework = argv[++i];
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
    request.project_output_dir = std::move(project_output_dir);
    request.project_name = std::move(project_name);
    request.target_framework = std::move(target_framework);
    request.options = options;

    if (!request.project_output_dir.empty()) {
        const auto result = csdecomp::decompile_to_project(request);
        if (!result.ok) {
            std::cerr << "Error: " << result.error_message << "\n";
            return 2;
        }
        std::cerr << "Visual Studio solution: " << result.solution_path << "\n";
        std::cerr << "Project file: " << result.project_path << "\n";
        std::cerr << "Source files: " << result.source_files.size() << "\n";
    } else if (request.output_path.empty()) {
        std::cout << csdecomp::decompile_to_string(request);
    } else {
        const auto result = csdecomp::decompile_to_file(request);
        if (!result.ok) {
            std::cerr << "Error: " << result.error_message << "\n";
            return 2;
        }
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
