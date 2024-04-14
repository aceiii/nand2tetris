#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <argparse/argparse.hpp>
#include <tl/expected.hpp>
#include <fstream>
#include <filesystem>

#include "vmtranslator.h"
#include "bootstrap.h"

tl::expected<std::string, std::string> get_file_contents(const std::istream& in) {
    if (!in) {
        return tl::unexpected(std::strerror(errno));
    }

    std::ostringstream contents;
    contents << in.rdbuf();
    return contents.str();
}

tl::expected<bool, std::string> write_file_contents(std::ostream& out, const std::vector<std::string>& lines) {
    if (!out) {
        return tl::unexpected(std::strerror(errno));
    }
    for (const auto &line : lines) {
        out.write(line.c_str(), line.size());
        out.write("\n", 1);
    }
    return true;
}

std::string replace_ext(const std::string& filename, const std::string& ext) {
    auto dot_index = filename.find_last_of(".");
    if (dot_index == std::string::npos) {
        return filename + "." + ext;
    }
    return filename.substr(0, dot_index) + "." + ext;
}

tl::expected<void, std::string> set_logging_level(const std::string& level) {
    if (level == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else if (level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (level == "info") {
        spdlog::set_level(spdlog::level::info);
    } else if (level == "warn") {
        spdlog::set_level(spdlog::level::warn);
    } else if (level == "err") {
        spdlog::set_level(spdlog::level::err);
    } else if (level == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else if (level == "off") {
        spdlog::set_level(spdlog::level::off);
    } else {
        return tl::unexpected(fmt::format("Invalid argument \"{}\" - allowed options: {{trace, debug, info, warn, err, critical, off}}", level));
    }
    return {};
}

auto main(int argc, char* argv[]) -> int {
    auto logger = spdlog::stderr_color_mt("stderr");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);\

    argparse::ArgumentParser program("assembler-cpp", "0.0.1");

    program.add_argument("-o", "--output")
        .help("File to output")
        .metavar("OUTPUT")
        .default_value("");

    program.add_argument("-l", "--log-level")
        .help("Set verbosity for logging")
        // .choices("trace", "debug", "info", "warn", "err", "critical", "off")
        .default_value(std::string("info"))
        .metavar("LEVEL")
        .nargs(1);

    program.add_argument("--stdin")
        .help("Read input from stdin")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("--stdout")
        .help("Output to stdout instead of file")
        .default_value(false)
        .implicit_value(true);

    program.add_argument("filename")
        .help("File to assemble.")
        .default_value("")
        .metavar("FILENAME")
        .nargs(1);

    auto args_error = [&] (const std::string& message) {
        std::cerr << message << std::endl;
        std::cerr << program;
        return 1;
    };

    try {
        program.parse_args(argc, argv);
    } catch(const std::exception& err) {
        return args_error(err.what());
    }

    const std::string level = program.get("--log-level");
    if (auto result = set_logging_level(level); !result.has_value()) {
        return args_error(result.error());
    }

    std::filesystem::path filepath(program.get("filename"));
    std::string output = program.get("--output");
    bool read_from_stdin = program.get<bool>("--stdin");
    bool write_to_stdout = program.get<bool>("--stdout");

    if (write_to_stdout && !output.empty()) {
        return args_error("May only use ONE OF --stdin or --output");
    }

    if ((read_from_stdin && !filepath.empty()) || (filepath.empty() && !read_from_stdin)) {
        return args_error("Must read from ONE of FILENAME or --stdin");
    }

    bool is_directory = !read_from_stdin && std::filesystem::is_directory(filepath);
    if (is_directory) {
        spdlog::info("Translating entire directory: {}", filepath.string());
    }

    if (output.empty() && !filepath.filename().empty() && !read_from_stdin) {
        output = replace_ext(filepath.filename(), "asm");
    } else if(output.empty()) {
        output = "out.asm";
    }

    VMTranslator translator;

    if (is_directory) {
        spdlog::debug("Adding boot assembly");
        if (const auto result = translator.add_boot_code(kDefaultBootstrapCode); !result.has_value()) {
            spdlog::error("Failed to add boot code: {}", result.error());
            return 1;
        }
    }

    if (is_directory) {
        for (auto const& dir_entry : std::filesystem::directory_iterator {filepath}) {
            if (!dir_entry.is_regular_file()) {
                continue;
            }

            if (dir_entry.path().extension() != ".vm") {
                continue;
            }

            const std::string filename = dir_entry.path();
            spdlog::info("Reading file: {}", filename);
            std::ifstream file(filename, std::ios::in);
            const auto contents = get_file_contents(file);

            if (!contents.has_value()) {
                spdlog::error("Failed to load file: {}", contents.error());
                return 1;
            }

            if (auto add_result = translator.add_file(dir_entry.path(), contents.value()); !add_result.has_value()) {
                spdlog::error("Add file failed: {}", add_result.error());
                return 1;
            }
        }
    } else {
        const auto contents = ([&] () {
            if (read_from_stdin) {
                spdlog::info("Reading from STDIN");
                return get_file_contents(std::cin);
            }

            const std::string filename = filepath;
            spdlog::info("Reading file: {}", filename);
            std::ifstream file(filename, std::ios::in);
            return get_file_contents(file);
        })();

        if (!contents.has_value()) {
            spdlog::error("Failed to load file: {}", contents.error());
            return 1;
        }

        if (auto add_result = translator.add_file(filepath.stem(), contents.value()); !add_result.has_value()) {
            spdlog::error("Add file failed: {}", add_result.error());
            return 1;
        }
    }

    auto result = translator.translate();
    if (!result.has_value()) {
        spdlog::error("Translation failed: {}", result.error());
        return 1;
    }

    auto write_result = ([&] () {
        if (write_to_stdout) {
            spdlog::info("Writing to STDOUT");
            return write_file_contents(std::cout, result.value());
        }

        spdlog::info("Writing to file: {}", output);

        std::fstream file(output, std::ios::out);
        return write_file_contents(file, result.value());
    })();

    if (!write_result.has_value()) {
        spdlog::error("Failed to write to file: {}", write_result.error());
        return 1;
    }

    return 0;
}
