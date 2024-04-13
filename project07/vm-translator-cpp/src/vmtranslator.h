#pragma once

#include <string>
#include <utility>
#include <vector>
#include <tl/expected.hpp>

class VMTranslator {
public:
    tl::expected<void, std::string> add_boot_assembly(const std::vector<std::string>& lines);
    tl::expected<void, std::string> add_file(const std::string& filename, const std::string& code);
    tl::expected<std::vector<std::string>, std::string> translate();

private:
    std::vector<std::pair<std::string, std::vector<std::string>>> files;
};
