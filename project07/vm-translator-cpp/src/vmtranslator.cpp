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
tl::expected<void, std::string> build_asm(const std::string& filename, const std::vector<vm_instruction>& instructions, std::vector<std::string>* out_lines);

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

std::string segment_name_string(const segment_pointer& seg) {
    switch (seg) {
        case kSegmentLocal: return "LCL";
        case kSegmentArgument: return "ARG";
        case kSegmentThis: return "THIS";
        case kSegmentThat: return "THAT";
        default: throw std::invalid_argument("Invalid segment");
    }
}

VMTranslator::VMTranslator(const std::string& filename, const std::string& code) : filename(filename) {
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

std::vector<std::string> tokenize(const std::string& line) {
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

    return tokens;
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

    std::vector<std::string> asm_lines;
    asm_lines.reserve(1024);

    auto result = build_asm(filename, instructions, &asm_lines);
    if (!result.has_value()) {
        return tl::unexpected(result.error());
    }

    return asm_lines;
}

tl::expected<segment_pointer, std::string> parse_segment_pointer(const std::string& segment) {
    if (segment == "local") {
        return kSegmentLocal;
    }
    if (segment == "argument") {
        return kSegmentArgument;
    }
    if (segment == "this") {
        return kSegmentThis;
    }
    if (segment == "that") {
        return kSegmentThat;
    }
    if (segment == "constant") {
        return kSegmentConstant;
    }
    if (segment == "static") {
        return kSegmentStatic;
    }
    if (segment == "pointer") {
        return kSegmentPointer;
    }
    if (segment == "temp") {
        return kSegmentTemp;
    }
    return tl::unexpected(fmt::format("Invalid segment: {}", segment));
}

tl::expected<uint16_t, std::string> parse_uint16_value(const std::string& num) {
    int value = stoi(num);
    if (value > 32767) {
        return tl::unexpected(fmt::format("Value '{}' exceeds maximum 32767", value));
    }
    uint16_t val = value;
    return val;
}

tl::expected<vm_instruction, std::string> parse_vm_line(const std::string& line) {
    std::vector<std::string> tokens = tokenize(line);

    if (tokens.empty()) {
        return tl::unexpected("Unexpected empty instruction line");
    }

    const std::string& cmd = tokens[0];

    if (tokens.size() == 1) {
        if (cmd == "add") {
            return cmd_arithmetic { kArithmeticOpAdd };
        }
        if (cmd == "sub") {
            return cmd_arithmetic { kArithmeticOpSub };
        }
        if (cmd == "neg") {
            return cmd_arithmetic { kArithmeticOpNeg };
        }
        if (cmd == "eq") {
            return cmd_arithmetic { kArithmeticOpEq };
        }
        if (cmd == "gt") {
            return cmd_arithmetic { kArithmeticOpGt };
        }
        if (cmd == "lt") {
            return cmd_arithmetic { kArithmeticOpLt };
        }
        if (cmd == "and") {
            return cmd_arithmetic { kArithmeticOpAnd };
        }
        if (cmd == "or") {
            return cmd_arithmetic { kArithmeticOpOr };
        }
        if (cmd == "not") {
            return cmd_arithmetic { kArithmeticOpNot };
        }
    }

    if (cmd == "push" && tokens.size() == 3) {
        const auto segment = parse_segment_pointer(tokens[1]);
        if (!segment.has_value()) {
            return tl::unexpected(segment.error());
        }

        const auto value = parse_uint16_value(tokens[2]);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        return cmd_push { segment.value(), value.value() };
    }

    if (cmd == "pop" && tokens.size() == 3) {
        const auto segment = parse_segment_pointer(tokens[1]);
        if (!segment.has_value()) {
            return tl::unexpected(segment.error());
        }

        const auto value = parse_uint16_value(tokens[2]);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        segment_pointer seg = segment.value();
        if (seg == kSegmentConstant) {
            return tl::unexpected("Constant segment cannot be used with pop command");
        }

        uint16_t val = value.value();

        return cmd_push { seg, value.value() };
    }

    return tl::unexpected(fmt::format("Unknown command: {}", line));
}

tl::expected<void, std::string> build_asm(const std::string& filename, const std::vector<vm_instruction>& instructions, std::vector<std::string>* out_lines) {
    std::string segment_name;
    for (const auto& instr : instructions) {
        auto res = std::visit(overloaded {
            [&] (const cmd_arithmetic& cmd) -> tl::expected<void, std::string> {
                switch (cmd.op)
                {
                case kArithmeticOpAdd:
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("D=M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("D=D+M");
                    return {};
                case kArithmeticOpSub: break;
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M-1");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("D=M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("A=M+1");
                    out_lines->push_back("A=M");
                    out_lines->push_back("D=D-A");
                    out_lines->push_back("@SP");
                    out_lines->push_back("A=M");
                    out_lines->push_back("M=D");
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                    return {};
                case kArithmeticOpNeg: break;
                case kArithmeticOpEq: break;
                case kArithmeticOpGt: break;
                case kArithmeticOpLt: break;
                case kArithmeticOpAnd: break;
                case kArithmeticOpOr: break;
                case kArithmeticOpNot: break;
                }
                return tl::unexpected("Not yet implemented");
            },
            [&] (const cmd_push& cmd) ->tl::expected<void, std::string> {
                try {
                    switch (cmd.seg)
                    {
                    case kSegmentConstant:
                        // RAM[SP] = i
                        out_lines->push_back(fmt::format("@{}", cmd.offset));
                        out_lines->push_back("D=A");
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=D");

                        // SP++
                        out_lines->push_back("@SP");
                        out_lines->push_back("M=M+1");

                        return {};

                    case kSegmentLocal:
                    case kSegmentArgument:
                    case kSegmentThis:
                    case kSegmentThat:
                        // addr <- segmentPointer + i
                        out_lines->push_back(fmt::format("@{}", cmd.offset));
                        out_lines->push_back("D=A");
                        out_lines->push_back(fmt::format("@{}", segment_name_string(cmd.seg)));
                        out_lines->push_back("D=D+M");

                        // RAM[SP] <- RAM[addr]
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=D");

                        // SP++
                        out_lines->push_back("@SP");
                        out_lines->push_back("M=M+1");

                        return {};

                    case kSegmentStatic:
                        {
                            if (cmd.offset >= 240) {
                                return tl::unexpected(fmt::format("Invalid Static offset: {}", cmd.offset));
                            }

                            std::string reg_name = fmt::format("{}.{}", filename, cmd.offset);

                            // RAM[SP] <- Foo.0
                            out_lines->push_back(fmt::format("@{}", reg_name));
                            out_lines->push_back("D=M");
                            out_lines->push_back("@SP");
                            out_lines->push_back("A=M");
                            out_lines->push_back("M=D");

                            // SP++
                            out_lines->push_back("@SP");
                            out_lines->push_back("M=M+1");

                            return {};
                        }

                    case kSegmentTemp:
                        {
                            if (cmd.offset >= 8) {
                                return tl::unexpected(fmt::format("Invalid temp offset: {}", cmd.offset));
                            }

                            int reg_val = 5 + cmd.offset;
                            std::string reg_name = fmt::format("R{}", reg_val);

                            // RAM[SP] <- R5
                            out_lines->push_back(fmt::format("@{}", reg_name));
                            out_lines->push_back("D=M");
                            out_lines->push_back("@SP");
                            out_lines->push_back("A=M");
                            out_lines->push_back("M=D");

                            // SP++
                            out_lines->push_back("@SP");
                            out_lines->push_back("M=M+1");

                            return {};
                        }

                    case kSegmentPointer:
                        {
                            std::string reg_name;
                            if (cmd.offset == 0) {
                                reg_name = "THIS";
                            } else if (cmd.offset == 1) {
                                reg_name = "THAT";
                            } else {
                                return tl::unexpected(fmt::format("Invalid pointer offset: {}", cmd.offset));
                            }

                            // RAM[SP] <- RAM[THIS]
                            out_lines->push_back(fmt::format("@{}", reg_name));
                            out_lines->push_back("A=M");
                            out_lines->push_back("D=M");
                            out_lines->push_back("@SP");
                            out_lines->push_back("A=M");
                            out_lines->push_back("M=D");

                            // SP++
                            out_lines->push_back("@SP");
                            out_lines->push_back("M=M+1");

                            return {};
                        }

                    default:
                        return tl::unexpected("Not yet implemented");
                    }
                } catch (const std::exception& err) {
                    return tl::unexpected(err.what());
                }
            },
            [&] (const cmd_pop& cmd) -> tl::expected<void, std::string> {
                try {
                    switch (cmd.seg)
                    {
                    case kSegmentConstant:
                        return tl::unexpected("Cannot pop constant segment");

                    case kSegmentLocal:
                    case kSegmentArgument:
                    case kSegmentThis:
                    case kSegmentThat:
                        // addr <- segmentPointer + i
                        out_lines->push_back(fmt::format("@{}", cmd.offset));
                        out_lines->push_back("D=A");
                        out_lines->push_back(fmt::format("@{}", segment_name_string(cmd.seg)));
                        out_lines->push_back("D=D+M");
                        out_lines->push_back("@R13");
                        out_lines->push_back("M=D");

                        // SP--
                        out_lines->push_back("@SP");
                        out_lines->push_back("AM=M-1");

                        // RAM[addr] <- RAM[SP]
                        out_lines->push_back("D=M");
                        out_lines->push_back("@R13");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=D");

                        return {};

                    case kSegmentStatic:
                        {
                            if (cmd.offset >= 240) {
                                return tl::unexpected(fmt::format("Invalid Static offset: {}", cmd.offset));
                            }

                            std::string reg_name = fmt::format("{}.{}", filename, cmd.offset);

                            // SP--
                            out_lines->push_back("@SP");
                            out_lines->push_back("AM=M-1");

                            //Foo.0 = RAM[SP]
                            out_lines->push_back("D=M");
                            out_lines->push_back(fmt::format("@{}", reg_name));
                            out_lines->push_back("M=D");

                            return {};
                        }

                    case kSegmentTemp:
                        {
                            if (cmd.offset >= 8) {
                                return tl::unexpected(fmt::format("Invalid temp offset: {}", cmd.offset));
                            }

                            int reg_val = 5 + cmd.offset;
                            const std::string reg_name = fmt::format("R{}", reg_val);

                            // SP--
                            out_lines->push_back("@SP");
                            out_lines->push_back("AM=M-1");

                            // R5 = RAM[SP]
                            out_lines->push_back("D=M");
                            out_lines->push_back(fmt::format("@{}", reg_name));
                            out_lines->push_back("M=D");

                            return {};
                        }

                    case kSegmentPointer:
                        {
                            std::string reg_name;
                            if (cmd.offset == 0) {
                                reg_name = "THIS";
                            } else if (cmd.offset == 1) {
                                reg_name = "THAT";
                            } else {
                                return tl::unexpected(fmt::format("Invalid pointer offset: {}", cmd.offset));
                            }

                            // SP--
                            out_lines->push_back("@SP");
                            out_lines->push_back("AM=M-1");

                            // RAM[THIS] = RAM[SP]
                            out_lines->push_back("D=M");
                            out_lines->push_back(fmt::format("@{}", reg_name));
                            out_lines->push_back("A=M");
                            out_lines->push_back("M=D");
                        }

                    default:
                        return tl::unexpected("Not yet implemented");
                    }
                } catch (const std::exception& err) {
                    return tl::unexpected(err.what());
                }
            },
            [] (auto&&) -> tl::expected<void, std::string> {
                return tl::unexpected("Not yet implemented");
            },
        }, instr);

        if (!res.has_value()) {
            return tl::unexpected(res.error());
        }
    }
    return {};
}
