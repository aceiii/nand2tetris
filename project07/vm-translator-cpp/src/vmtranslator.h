#pragma once

#include <string>
#include <vector>
#include <tl/expected.hpp>

class VMTranslator {
public:
    VMTranslator(const std::string& code);

    tl::expected<std::vector<std::string>, std::string> translate();

private:
    std::string code;
};
