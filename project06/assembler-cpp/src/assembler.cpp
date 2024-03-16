#include "assembler.h"

#include <spdlog/spdlog.h>
#include <sstream>
#include <vector>
#include <map>
#include <regex>

struct instr_empty {};

struct instr_label {
    std::string label;
};

struct instr_a {
    std::string value;
};

struct instr_c {
    std::string dest;
    std::string src;
    std::string op;
};

using instr_line = std::variant<instr_empty, instr_label, instr_a, instr_c>;

tl::expected<instr_line, std::string> parse_instruction_line(std::string line);
uint8_t assemble_instruction_line(const instr_line& instr, std::map<std::string, uint16_t>& symbol_map, uint16_t& next_register);

std::string trim_whitespace(const std::string& str) {
    const std::string whitespace = " \t";
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

Assembler::Assembler(const std::string& code) : code(code) {}

tl::expected<bytes, std::string> Assembler::parse() {
    std::stringstream ss(code);
    std::string line;

    std::vector<instr_line> instructions;
    std::map<std::string, uint16_t> symbol_map = {
        { "R0", 0 },
        { "R1", 1 },
        { "R2", 2 },
        { "R3", 3 },
        { "R4", 4 },
        { "R5", 5 },
        { "R6", 6 },
        { "R7", 7 },
        { "R8", 8 },
        { "R9", 9 },
        { "R10", 10 },
        { "R11", 11 },
        { "R12", 12 },
        { "R13", 13 },
        { "R14", 14 },
        { "R15", 15 },
        { "SCREEN", 16384 },
        { "KBD", 24576 },
    };

    while (std::getline(ss, line, '\n')) {
        auto result = parse_instruction_line(line);
        if (!result.has_value()) {
            return tl::unexpected(result.error());
        }

        auto instr = result.value();
        if (std::holds_alternative<instr_empty>(instr)) {
            continue;
        } else if (std::holds_alternative<instr_label>(instr)) {
            symbol_map[std::get<instr_label>(instr).label] = instructions.size();
            continue;
        }
        instructions.push_back(instr);
    }

    bytes buffer;
    buffer.reserve(1024);
    uint16_t next_register = 16;

    for (const auto& instr: instructions) {
        buffer.push_back(assemble_instruction_line(instr, symbol_map, next_register));
    }

    spdlog::info("Generated {} bytes of hack", buffer.size());

    return buffer;
}

tl::expected<instr_line, std::string> parse_instruction_line(std::string line) {
    spdlog::trace(">>>  {}", line);

    line = trim_whitespace(trim_comments(line));
    if (line.empty()) {
        return instr_empty {};
    }

    if (line[0] == '@') {
        return instr_a { line.substr(1) };
    }

    return tl::unexpected("Not yet implemented!");
};

uint8_t assemble_instruction_line(const instr_line& instr, std::map<std::string, uint16_t>& symbol_map, uint16_t& next_register) {
    return 0;
}
