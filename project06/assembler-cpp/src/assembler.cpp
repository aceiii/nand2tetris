#include <spdlog/spdlog.h>

#include "assembler.h"

Assembler::Assembler(const std::string& code) : code(code) {}

tl::expected<bytes, std::string> Assembler::parse() {
    bytes buffer;
    buffer.reserve(1024);

    // return tl::unexpected("Something failed");

    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    buffer.push_back(4);

    return buffer;
}
