#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <tl/expected.hpp>
#include <filesystem>
#include <fstream>

#include "assembler.h"

tl::expected<bool, std::string> write_asm_to_file(const std::string& filename, const bytes& buffer) {
    std::ofstream out(filename, std::ios::out | std::ios::binary);
    if (out) {
        for (const auto &byte : buffer) {
            out.write((char*)&byte, sizeof(byte));
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

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::trace);

    argparse::ArgumentParser program("assembler-cpp", "0.0.1");

    program.add_argument("filename")
        .help("File to assemble.")
        .metavar("FILENAME");

    program.add_argument("-o", "--output")
        .help("File to output")
        .metavar("OUTPUT")
        .default_value("");

    try {
        program.parse_args(argc, argv);
    } catch(const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    std::string filename = program.get("filename");
    std::string output = program.get("--output");

    if (output.empty()) {
        output = replace_ext(filename, "hack");
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
