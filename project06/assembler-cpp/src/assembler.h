#pragma once

#include <string>
#include <vector>
#include <tl/expected.hpp>

typedef std::vector<uint8_t> bytes;

class Assembler {
public:
    Assembler(const std::string& code);

    tl::expected<bytes, std::string> parse();

private:
    std::string code;
};
