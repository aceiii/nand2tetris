#pragma once

#include <string>
#include <vector>
#include <tl/expected.hpp>

using buffer = std::vector<uint16_t>;

class Assembler {
public:
    Assembler(const std::string& code);

    tl::expected<buffer, std::string> parse();

private:
    std::string code;
};
