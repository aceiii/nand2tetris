#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <argparse/argparse.hpp>
#include <tl/expected.hpp>
#include <fstream>
#include <filesystem>

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
        output = replace_ext(std::filesystem::path(filename).filename(), "asm");
    } else {
        output = "out.asm";
    }

    return 0;
}
