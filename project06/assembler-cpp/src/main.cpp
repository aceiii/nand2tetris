#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <tl/expected.hpp>
#include <filesystem>
#include <fstream>

#include "assembler.h"

tl::expected<bool, std::string> write_asm_to_file(const std::string& filename, const buffer& buf) {
    std::ofstream out(filename, std::ios::out);
    if (out) {
        for (const auto &byte : buf) {
            // out.write((char*)&byte, sizeof(byte));
            std::string line = fmt::format("{:016b}\n", byte);
            out.write(line.c_str(), line.size());
        }
        out.close();
        return true;
    }
    return tl::unexpected(std::strerror(errno));
}

tl::expected<std::string, std::string> get_file_contents(const std::string& filename) {
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in)
    {
        std::ostringstream contents;
        contents << in.rdbuf();
        in.close();
        return(contents.str());
    }
    return tl::unexpected(std::strerror(errno));
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
        .metavar("LEVEL");

    program.add_argument("filename")
        .help("File to assemble.")
        .metavar("FILENAME")
        .nargs(1);

    try {
        program.parse_args(argc, argv);
    } catch(const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    const std::string level = program.get("--log-level");
    if (auto result = set_logging_level(level); !result.has_value()) {
        std::cerr << result.error() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::string filename = program.get("filename");
    std::string output = program.get("--output");

    if (output.empty()) {
        output = replace_ext(std::filesystem::path(filename).filename(), "hack");
    }

    spdlog::info("Reading file: {}", filename);
    const auto contents = get_file_contents(filename);

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

    spdlog::info("Writing to file: {}", output);
    if (auto write_result = write_asm_to_file(output, result.value()); !write_result.has_value()) {
        spdlog::error("Failed to write to file: {}", write_result.error());
    }

    return 0;
}
