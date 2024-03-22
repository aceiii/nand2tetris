#include "vmtranslator.h"

#include <spdlog/spdlog.h>
#include <sstream>

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

std::string trim_whitespace(const std::string& str) {
    const std::string whitespace = " \t\r";
    const auto begin = str.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = str.find_last_not_of(whitespace);
    return str.substr(begin, end - begin + 1);
}

std::string trim_comments(const std::string& str) {
    const auto pos = str.find("//");
    if (pos == std::string::npos) {
        return str;
    }
    return str.substr(0, pos);
}

std::string remove_whitespace(const std::string& str) {
    std::string out = str;
    auto pos = std::remove_if(out.begin(), out.end(), [] (const auto& c) {
        return c == ' ' || c == '\t' || c == '\r';
    });
    out.erase(pos, out.end());
    return out;
}

VMTranslator::VMTranslator(const std::string& code) {
    std::stringstream ss(code);
    std::string line;

    while(std::getline(ss, line)) {
        line = trim_whitespace(trim_comments(line));
        if (line.empty()) {
            continue;
        }
        lines.push_back(line);
    }
}

tl::expected<std::vector<std::string>, std::string> VMTranslator::translate() {
    for (const auto &line : lines) {
        spdlog::trace(">>> {}", line);
    }

    return tl::unexpected("Not Implemented!");
}