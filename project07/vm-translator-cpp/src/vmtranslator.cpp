#include "vmtranslator.h"

#include <spdlog/spdlog.h>
#include <sstream>

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

enum arithmetic_op {
    kArithmeticOpAdd,
    kArithmeticOpSub,
    kArithmeticOpNeg,
    kArithmeticOpEq,
    kArithmeticOpGt,
    kArithmeticOpLt,
    kArithmeticOpAnd,
    kArithmeticOpOr,
    kArithmeticOpNot,
};

enum segment_pointer {
    kSegmentLocal,
    kSegmentArgument,
    kSegmentStatic,
    kSegmentConstant,
    kSegmentThis,
    kSegmentThat,
    kSegmentPointer,
    kSegmentTemp,
};

struct cmd_arithmetic {
    arithmetic_op op;
};

struct cmd_push {
    segment_pointer seg;
    uint16_t offset;
};

struct cmd_pop {
    segment_pointer seg;
    uint16_t offset;
};

struct cmd_label {
    std::string label;
};

struct cmd_goto {
    std::string label;
};

struct cmd_if {
};

struct cmd_function {
};

struct cmd_return {
};

struct cmd_call {
};

using vm_instruction = std::variant<cmd_arithmetic, cmd_push, cmd_pop, cmd_label, cmd_goto, cmd_if, cmd_function, cmd_return, cmd_call>;

tl::expected<vm_instruction, std::string> parse_vm_line(const std::string& line);

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
    std::vector<vm_instruction> instructions;

    for (const auto &line : lines) {
        spdlog::trace(">>> {}", line);
        auto result = parse_vm_line(line);
        if (!result.has_value()) {
            return tl::unexpected(result.error());
        }
        instructions.push_back(result.value());
    }

    return tl::unexpected("Not Implemented!");
}

tl::expected<vm_instruction, std::string> parse_vm_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;

    while (getline(ss, token, ' ')) {
        token = trim_whitespace(token);
        if (token.empty()) {
            continue;
        }
        tokens.push_back(token);
    }

    if (tokens.empty()) {
        return tl::unexpected("Unexpected empty instruction line");
    }

    token = tokens[0];
    if (tokens.size() == 1) {
        if (token == "add") {
            return cmd_arithmetic { kArithmeticOpAdd };
        }
        if (token == "sub") {
            return cmd_arithmetic { kArithmeticOpSub };
        }
        if (token == "neg") {
            return cmd_arithmetic { kArithmeticOpNeg };
        }
        if (token == "eq") {
            return cmd_arithmetic { kArithmeticOpEq };
        }
        if (token == "gt") {
            return cmd_arithmetic { kArithmeticOpGt };
        }
        if (token == "lt") {
            return cmd_arithmetic { kArithmeticOpLt };
        }
        if (token == "and") {
            return cmd_arithmetic { kArithmeticOpAnd };
        }
        if (token == "or") {
            return cmd_arithmetic { kArithmeticOpOr };
        }
        if (token == "not") {
            return cmd_arithmetic { kArithmeticOpNot };
        }
    }

    if (token == "push" && tokens.size() == 3) {
        const std::string& segment = tokens[1];
        const std::string& num = tokens[2];

        segment_pointer seg;

        if (segment == "local") {
            seg = kSegmentLocal;
        } else if (segment == "argument") {
            seg = kSegmentArgument;
        } else if (segment == "this") {
            seg = kSegmentThis;
        } else if (segment == "that") {
            seg = kSegmentThat;
        } else if (segment == "constant") {
            seg = kSegmentConstant;
        } else if (segment == "static") {
            seg = kSegmentStatic;
        } else if (segment == "pointer") {
            seg = kSegmentPointer;
        } else if (segment == "temp") {
            seg = kSegmentTemp;
        } else {
            return tl::unexpected(fmt::format("Invalid segment: {}", segment));
        }

        if (!isdigit(num[0])) {
            return tl::unexpected(fmt::format("Invalid value: {}", tokens[2]));
        }

        int value = stoi(num);
        if (value > 32767) {
            return tl::unexpected(fmt::format("Value '{}' exceeds maximum 32767", value));
        }

        uint16_t val = value;
        return cmd_push { seg, val };
    }

    if (token == "pop" && tokens.size() == 3) {
        const std::string& segment = tokens[1];
        const std::string& num = tokens[2];

        segment_pointer seg;
        if (segment == "local") {
            seg = kSegmentLocal;
        } else if (segment == "argument") {
            seg = kSegmentArgument;
        } else if (segment == "this") {
            seg = kSegmentThis;
        } else if (segment == "that") {
            seg = kSegmentThat;
        } else if (segment == "constant") {
            seg = kSegmentConstant;
        } else if (segment == "static") {
            seg = kSegmentStatic;
        } else if (segment == "pointer") {
            seg = kSegmentPointer;
        } else if (segment == "temp") {
            seg = kSegmentTemp;
        } else {
            return tl::unexpected(fmt::format("Invalid segment: {}", segment));
        }

        if (seg == kSegmentConstant) {
            return tl::unexpected("Constant segment cannot be used with pop command");
        }

        int value = stoi(num);
        if (value > 32767) {
            return tl::unexpected(fmt::format("Value '{}' exceeds maximum 32767", value));
        }

        uint16_t val = value;
        return cmd_push { seg, val };
    }

    return tl::unexpected(fmt::format("Unknown command: {}", line));
}
