#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <argparse/argparse.hpp>
#include <tl/expected.hpp>
#include <filesystem>
#include <fstream>

#include "assembler.h"

tl::expected<bool, std::string> write_asm_to_file(std::ostream& out, const buffer& buf, bool binary) {
    if (!out) {
        return tl::unexpected(std::strerror(errno));
    }
    if (binary) {
        for (const auto &byte : buf) {
            out.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
        }
    } else {
        for (const auto &byte : buf) {
            std::string line = fmt::format("{:016b}\n", byte);
            out.write(line.c_str(), line.size());
        }
    }
    return true;
}

tl::expected<std::string, std::string> get_file_contents(std::istream& in) {
    // std::ifstream in(filename, std::ios::in);
    if (!in) {
        return tl::unexpected(std::strerror(errno));
    }

    std::ostringstream contents;
    contents << in.rdbuf();
    return(contents.str());
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

int main(int argc, char* argv[]) {
    auto logger = spdlog::stderr_color_mt("stderr");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

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

    program.add_argument("-b", "--binary")
        .help("Output to binary file")
        .default_value(false)
        .implicit_value(true);

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

    std::string filename = program.get("filename");
    std::string output = program.get("--output");
    bool read_from_stdin = program.get<bool>("--stdin");
    bool write_to_stdout = program.get<bool>("--stdout");

    if (write_to_stdout && !output.empty()) {
        return args_error("May only use ONE OF --stdin or --output");
    }

    if ((read_from_stdin && !filename.empty()) || (filename.empty() && !read_from_stdin)) {
        return args_error("Must read from ONE of FILENAME or --stdin");
    }

    if (output.empty() && !read_from_stdin) {
        output = replace_ext(std::filesystem::path(filename).filename(), "hack");
    } else {
        output = "out.hack";
    }

    const auto contents = ([&] () {
        if (read_from_stdin) {
            spdlog::info("Reading from STDIN");
            return get_file_contents(std::cin);
        }

        spdlog::info("Reading file: {}", filename);
        std::ifstream file(filename, std::ios::in);
        return get_file_contents(file);
    })();

    if (!contents.has_value()) {
        spdlog::error("Failed to load file: {}", contents.error());
        return 1;
    }

    Assembler assembler(contents.value());
    const auto result = assembler.parse();
    if (!result.has_value()) {
        spdlog::error("Parse failed: {}", result.error());
        return 1;
    }

    auto write_result = ([&] () {
        bool write_binary = program.get<bool>("--binary");

        if (write_to_stdout) {
            spdlog::info("Writing to STDOUT");
            return write_asm_to_file(std::cout, result.value(), write_binary);
        }

        spdlog::info("Writing to file: {}", output);

        auto flags = std::ios::out;
        if (write_binary) {
            flags |= std::ios::binary;
        }

        std::ofstream file(output, flags);
        return write_asm_to_file(file, result.value(), write_binary);
        file.close();
    })();

    if (!write_result.has_value()) {
        spdlog::error("Failed to write to file: {}", write_result.error());
        return 1;
    }

    return 0;
}
