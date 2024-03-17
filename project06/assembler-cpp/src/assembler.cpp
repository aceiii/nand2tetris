#include "assembler.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct instr_empty {};

struct instr_label {
    std::string label;
};

struct instr_a {
    std::string value;
};

struct instr_c {
    std::string dest;
    std::string comp;
    std::string jump;
};

using instr_line = std::variant<instr_empty, instr_label, instr_a, instr_c>;
using instruction = std::variant<instr_a, instr_c>;

tl::expected<instr_line, std::string> parse_instruction_line(std::string line);
tl::expected<uint16_t, std::string> assemble_instruction_line(const instruction& instr, std::map<std::string, uint16_t>& symbol_map, uint16_t& next_register);

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

Assembler::Assembler(const std::string& code) : code(code) {}

tl::expected<buffer, std::string> Assembler::parse() {
    std::stringstream ss(code);
    std::string line;

    std::vector<instruction> instructions;
    std::map<std::string, uint16_t> symbol_map = {
        { "SP",     0 },
        { "LCL",    1 },
        { "ARG",    2 },
        { "THIS",   3 },
        { "THAT",   4 },
        { "R0",     0 },
        { "R1",     1 },
        { "R2",     2 },
        { "R3",     3 },
        { "R4",     4 },
        { "R5",     5 },
        { "R6",     6 },
        { "R7",     7 },
        { "R8",     8 },
        { "R9",     9 },
        { "R10",    10 },
        { "R11",    11 },
        { "R12",    12 },
        { "R13",    13 },
        { "R14",    14 },
        { "R15",    15 },
        { "SCREEN", 16384 },
        { "KBD",    24576 },
    };

    while (std::getline(ss, line)) {
        auto result = parse_instruction_line(line);
        if (!result.has_value()) {
            return tl::unexpected(result.error());
        }

        auto instr = result.value();
        std::visit(overloaded {
            [] (const instr_empty&) {},
            [&] (const instr_label& instr) { symbol_map[instr.label] = instructions.size(); },
            [&] (const instr_a& a) { instructions.push_back(a); },
            [&] (const instr_c& c) { instructions.push_back(c); },
        }, instr);
    }

    buffer buf;
    buf.reserve(1024);
    uint16_t next_register = 16;

    for (const auto& instr: instructions) {
        auto result = assemble_instruction_line(instr, symbol_map, next_register);
        if (!result.has_value()) {
            return tl::unexpected(result.error());
        }

        buf.push_back(result.value());
    }

    spdlog::info("Generated {} bytes of hack", buf.size());

    return buf;
}

tl::expected<instr_line, std::string> parse_instruction_line(std::string line) {
    spdlog::trace(">>> {}", line);

    line = trim_whitespace(trim_comments(line));
    if (line.empty()) {
        return instr_empty {};
    }

    if (line[0] == '@') {
        return instr_a { line.substr(1) };
    }

    if (line[0] == '(') {
        if (line.back() != ')') {
            return tl::unexpected(fmt::format("Unexpected instruction: {}", line));
        }

        std::string label = line.substr(1, line.size() - 2);
        return instr_label { label };
    }

    std::string dest, comp, jump;
    auto eq_pos = line.find_first_of("=");
    auto semi_pos = line.find_last_of(";");

    if (eq_pos == std::string::npos && semi_pos == std::string::npos) {
        comp = line;
    } else if (eq_pos == std::string::npos) {
        comp = line.substr(0, semi_pos);
        jump = line.substr(semi_pos + 1);
    } else if (semi_pos == std::string::npos) {
        dest = line.substr(0, eq_pos);
        comp = line.substr(eq_pos + 1);
    } else {
        dest = line.substr(0, eq_pos);
        comp = line.substr(eq_pos + 1, semi_pos);
        jump = line.substr(semi_pos + 1);
    }
    return instr_c { dest, comp, jump };
};


static std::map<std::string, uint16_t> dest_map = {
    { "",    0b000 },
    { "M",   0b001 },
    { "D",   0b010 },
    { "DM",  0b011 },
    { "MD",  0b011 },
    { "A",   0b100 },
    { "AM",  0b101 },
    { "MA",  0b101 },
    { "AD",  0b110 },
    { "DA",  0b110 },
    { "ADM", 0b111 },
    { "AMD", 0b111 },
    { "DAM", 0b111 },
    { "DMA", 0b111 },
    { "MAD", 0b111 },
    { "MDA", 0b111 },
};

static std::map<std::string, uint16_t> comp_map = {
    { "0",    0b0101010 },
    { "1",    0b0111111 },
    { "-1",   0b0111010 },
    { "D",    0b0001100 },
    { "A",    0b0110000 },
    { "M",    0b1110000 },
    { "!D",   0b0001101 },
    { "!A",   0b0110001 },
    { "!M",   0b1110001 },
    { "-D",   0b0001111 },
    { "-A",   0b0110011 },
    { "-M",   0b1110011 },
    { "D+1",  0b0011111 },
    { "A+1",  0b0110111 },
    { "M+1",  0b1110111 },
    { "D-1",  0b0001110 },
    { "A-1",  0b0110010 },
    { "M-1",  0b1110010 },
    { "D+A",  0b0000010 },
    { "D+M",  0b1000010 },
    { "D-A",  0b0010011 },
    { "D-M",  0b1010011 },
    { "A-D",  0b0000111 },
    { "M-D",  0b1000111 },
    { "D&A",  0b0000000 },
    { "D&M",  0b1000000 },
    { "D|A",  0b0010101 },
    { "D|M",  0b1010101 },
};

static std::map<std::string, uint16_t> jump_map = {
    { "",    0b000 },
    { "JGT", 0b001 },
    { "JEQ", 0b010 },
    { "JGE", 0b011 },
    { "JLT", 0b100 },
    { "JNE", 0b101 },
    { "JLE", 0b110 },
    { "JMP", 0b111 },
};

tl::expected<uint16_t, std::string> assemble_instruction_line(const instruction& instr, std::map<std::string, uint16_t>& symbol_map, uint16_t& next_register) {
    if (std::holds_alternative<instr_a>(instr)) {
        const auto& a = std::get<instr_a>(instr);
        spdlog::trace("A-instr: {}", a.value);

        if (isdigit(a.value[0])) {
            int value = stoi(a.value);
            if (value > 32767) {
                return tl::unexpected(fmt::format("A-instruction constant value '{}' exceeds maximum 32767", value));
            }
            return value;
        }

        auto found = symbol_map.find(a.value);
        if (found == symbol_map.end()) {
            uint16_t register_value = next_register++;
            symbol_map[a.value] = register_value;
            return register_value;
        }

        return found->second;
    } else {
        const auto& c = std::get<instr_c>(instr);
        std::string dest = remove_whitespace(c.dest);
        std::string comp = remove_whitespace(c.comp);
        std::string jump = remove_whitespace(c.jump);

        spdlog::trace("C-instr: [{}, {}, {}]", dest, comp, jump);

        uint16_t jbits, dbits, cbits;

        try {
            cbits = comp_map.at(comp);
        } catch (const std::exception&) {
            return tl::unexpected(fmt::format("Invalid COMP: {}", comp));
        }

        try {
            dbits = dest_map.at(dest);
        } catch (const std::exception&) {
            return tl::unexpected(fmt::format("Invalid DEST: {}", dest));
        }

        try {
            jbits = jump_map.at(jump);
        } catch (const std::exception&) {
            return tl::unexpected(fmt::format("Invalid JUMP: {}", jump));
        }

        return jbits | (dbits << 3) | (cbits << 6) | (0b111 << 13);
    }
}
