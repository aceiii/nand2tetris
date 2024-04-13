#include "vmtranslator.h"

#include <spdlog/spdlog.h>
#include <sstream>
#include <utility>

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
    std::string label;
};

struct cmd_function {
    std::string name;
    uint8_t count;
};

struct cmd_return {
};

struct cmd_call {
    std::string name;
    uint8_t count;
};

using vm_instruction = std::variant<cmd_arithmetic, cmd_push, cmd_pop, cmd_label, cmd_goto, cmd_if, cmd_function, cmd_return, cmd_call>;

tl::expected<vm_instruction, std::string> parse_vm_line(const std::string& filename, const std::string& line);
tl::expected<void, std::string> build_asm(const std::string& filename, const std::vector<std::pair<vm_instruction, std::string>>& instructions, std::vector<std::string>* out_lines);

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

tl::expected<void, std::string> VMTranslator::add_boot_assembly(const std::vector<std::string>& lines) {
    // TODO: copy boot assembly to output
    return {};
}

tl::expected<void, std::string> VMTranslator::add_file(const std::string& filename, const std::string& code) {
    std::vector<std::string> lines;

    std::stringstream ss(code);
    std::string line;

    while(std::getline(ss, line)) {
        line = trim_whitespace(trim_comments(line));
        if (line.empty()) {
            continue;
        }
        lines.emplace_back(line);
    }

    files.emplace_back(std::make_pair(filename, std::move(lines)));

    return {};
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
    std::vector<std::string> asm_lines;
    asm_lines.reserve(1024);

    for (const auto &[filename, lines] : files) {
        std::vector<std::pair<vm_instruction, std::string>> instructions;

        for (const auto &line : lines) {
            spdlog::trace(">>> {}", line);
            auto result = parse_vm_line(filename, line);
            if (!result.has_value()) {
                return tl::unexpected(result.error());
            }
            instructions.push_back(std::make_pair(result.value(), line));
        }

        auto result = build_asm(filename, instructions, &asm_lines);
        if (!result.has_value()) {
            return tl::unexpected(result.error());
        }
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
    if (value > 32767 || value < -32768) {
        return tl::unexpected(fmt::format("Value '{}' not in range [-32768, 32767]", value));
    }
    uint16_t val = value;
    return val;
}

tl::expected<uint8_t, std::string> parse_uint8_value(const std::string& num) {
    int value = stoi(num);
    if (value > 127 || value < -128) {
        return tl::unexpected(fmt::format("Value '{}' not in range [-128, 127]", value));
    }
    uint8_t val = value;
    return val;
}

tl::expected<vm_instruction, std::string> parse_vm_line(const std::string& filename, const std::string& line) {
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

        return cmd_pop { seg, value.value() };
    }

    if (cmd == "label" && tokens.size() == 2) {
        return cmd_label { tokens[1] };
    }

    if (cmd == "goto" && tokens.size() == 2) {
        return cmd_goto { tokens[1] };
    }

    if (cmd == "if-goto" && tokens.size() == 2) {
        return cmd_if { tokens[1] };
    }

    if (cmd == "function" && tokens.size() == 3) {
        const auto value = parse_uint8_value(tokens[2]);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        return cmd_function { tokens[1], value.value() };
    }

    if (cmd == "call" && tokens.size() == 3) {
        const auto value = parse_uint8_value(tokens[2]);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        return cmd_call { tokens[1], value.value() };
    }

    if (cmd == "return" && tokens.size() == 1) {
        return cmd_return {};
    }

    return tl::unexpected(fmt::format("Unknown command: {}", line));
}

tl::expected<void, std::string> build_asm(const std::string& filename, const std::vector<std::pair<vm_instruction, std::string>>& instructions, std::vector<std::string>* out_lines) {
    std::string segment_name;
    int counter = 0;
    for (const auto& instr_pair : instructions) {
        const vm_instruction& instr = std::get<0>(instr_pair);
        const std::string& line = std::get<1>(instr_pair);

        out_lines->push_back(fmt::format("// {}", line));

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
                    out_lines->push_back("M=D+M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                    return {};
                case kArithmeticOpSub:
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("D=M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("M=M-D");
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                    return {};
                case kArithmeticOpNeg:
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("M=-M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                    return {};
                case kArithmeticOpEq:
                    {
                        std::string label = fmt::format("kArithmeticOpEq.{}", ++counter);

                        out_lines->push_back("@SP");
                        out_lines->push_back("AM=M-1");
                        out_lines->push_back("D=M");
                        out_lines->push_back("@SP");
                        out_lines->push_back("AM=M-1");
                        out_lines->push_back("D=M-D");
                        out_lines->push_back(fmt::format("@{}", label));
                        out_lines->push_back("D;JEQ");
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=0");
                        out_lines->push_back(fmt::format("@{}.end", label));
                        out_lines->push_back("0;JMP");
                        out_lines->push_back(fmt::format("({})", label));
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=-1");
                        out_lines->push_back(fmt::format("({}.end)", label));
                        out_lines->push_back("@SP");
                        out_lines->push_back("M=M+1");

                        return {};
                    }
                case kArithmeticOpGt:
                    {
                        std::string label = fmt::format("kArithmeticOpGt.{}", ++counter);

                        out_lines->push_back("@SP");
                        out_lines->push_back("AM=M-1");
                        out_lines->push_back("D=M");
                        out_lines->push_back("@SP");
                        out_lines->push_back("AM=M-1");
                        out_lines->push_back("D=M-D");
                        out_lines->push_back(fmt::format("@{}", label));
                        out_lines->push_back("D;JGT");
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=0");
                        out_lines->push_back(fmt::format("@{}.end", label));
                        out_lines->push_back("0;JMP");
                        out_lines->push_back(fmt::format("({})", label));
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=-1");
                        out_lines->push_back(fmt::format("({}.end)", label));
                        out_lines->push_back("@SP");
                        out_lines->push_back("M=M+1");

                        return {};
                    }
                case kArithmeticOpLt:
                    {
                        std::string label = fmt::format("kArithmeticOpGt.{}", ++counter);

                        out_lines->push_back("@SP");
                        out_lines->push_back("AM=M-1");
                        out_lines->push_back("D=M");
                        out_lines->push_back("@SP");
                        out_lines->push_back("AM=M-1");
                        out_lines->push_back("D=M-D");
                        out_lines->push_back(fmt::format("@{}", label));
                        out_lines->push_back("D;JLT");
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=0");
                        out_lines->push_back(fmt::format("@{}.end", label));
                        out_lines->push_back("0;JMP");
                        out_lines->push_back(fmt::format("({})", label));
                        out_lines->push_back("@SP");
                        out_lines->push_back("A=M");
                        out_lines->push_back("M=-1");
                        out_lines->push_back(fmt::format("({}.end)", label));
                        out_lines->push_back("@SP");
                        out_lines->push_back("M=M+1");

                        return {};
                    }
                case kArithmeticOpAnd:
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("D=M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("M=D&M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                    return {};
                case kArithmeticOpOr:
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("D=M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("M=D|M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                    return {};
                case kArithmeticOpNot:
                    out_lines->push_back("@SP");
                    out_lines->push_back("AM=M-1");
                    out_lines->push_back("M=!M");
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                    return {};
                }
                return tl::unexpected(fmt::format("Not Implemented: {}", line));
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
                        out_lines->push_back(fmt::format("@{}", segment_name_string(cmd.seg)));
                        out_lines->push_back("D=M");
                        out_lines->push_back(fmt::format("@{}", cmd.offset));
                        out_lines->push_back("A=D+A");
                        out_lines->push_back("D=M");

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

                            // RAM[SP] <- THIS
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

                    default:
                        return tl::unexpected(fmt::format("Not Implemented: {}", line));
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
                        out_lines->push_back(fmt::format("@{}", segment_name_string(cmd.seg)));
                        out_lines->push_back("D=M");
                        out_lines->push_back(fmt::format("@{}", cmd.offset));
                        out_lines->push_back("D=D+A");
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

                            // THIS = RAM[SP]
                            out_lines->push_back("D=M");
                            out_lines->push_back(fmt::format("@{}", reg_name));
                            out_lines->push_back("M=D");

                            return {};
                        }

                    default:
                        return tl::unexpected(fmt::format("Not Implemented: {}", line));
                    }
                } catch (const std::exception& err) {
                    return tl::unexpected(err.what());
                }
            },
            [&] (const cmd_label& cmd) -> tl::expected<void, std::string> {
                out_lines->push_back(fmt::format("({})", cmd.label));
                return {};
            },
            [&] (const cmd_goto& cmd) -> tl::expected<void, std::string> {
                out_lines->push_back(fmt::format("@{}", cmd.label));
                out_lines->push_back("0;JMP");
                return {};
            },
            [&] (const cmd_if& cmd) -> tl::expected<void, std::string> {
                // SP--
                out_lines->push_back("@SP");
                out_lines->push_back("AM=M-1");

                // pop D
                out_lines->push_back("D=M");

                // jump if D != 0
                out_lines->push_back(fmt::format("@{}", cmd.label));
                out_lines->push_back("D;JNE");

                return {};
            },
            [&] (const cmd_function& cmd) -> tl::expected<void, std::string> {

                // function label
                out_lines->push_back(fmt::format("({})", cmd.name));

                // initialize local vars
                for (int i = 0; i < cmd.count; i += 1) {
                    out_lines->push_back("@SP");
                    out_lines->push_back("A=M");
                    out_lines->push_back("M=0");

                    // SP++
                    out_lines->push_back("@SP");
                    out_lines->push_back("M=M+1");
                }

                return {};
            },
            [&] (const cmd_return& cmd) -> tl::expected<void, std::string> {
                // endFrame (R13) = LCL
                out_lines->push_back("@LCL");
                out_lines->push_back("D=M");
                out_lines->push_back("@R13");
                out_lines->push_back("M=D");

                // retAddr (R14) = RAM[endFrame - 5]
                out_lines->push_back("@5");
                out_lines->push_back("A=D-A");
                out_lines->push_back("D=M");
                out_lines->push_back("@R14");
                out_lines->push_back("M=D");

                // RAM[ARG] <- RAM[SP-1]
                out_lines->push_back("@SP");
                out_lines->push_back("A=M-1");
                out_lines->push_back("D=M");
                out_lines->push_back("@ARG");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");

                // SP = ARG + 1
                out_lines->push_back("@ARG");
                out_lines->push_back("D=M+1");
                out_lines->push_back("@SP");
                out_lines->push_back("M=D");

                // THAT = RAM[endFrame - 1]
                out_lines->push_back("@R13");
                out_lines->push_back("A=M-1");
                out_lines->push_back("D=M");
                out_lines->push_back("@THAT");
                out_lines->push_back("M=D");

                // THIS = RAM[endFrame - 2]
                out_lines->push_back("@2");
                out_lines->push_back("D=A");
                out_lines->push_back("@R13");
                out_lines->push_back("A=M-D");
                out_lines->push_back("D=M");
                out_lines->push_back("@THIS");
                out_lines->push_back("M=D");

                // ARG = RAM[endFrame - 3]
                out_lines->push_back("@3");
                out_lines->push_back("D=A");
                out_lines->push_back("@R13");
                out_lines->push_back("A=M-D");
                out_lines->push_back("D=M");
                out_lines->push_back("@ARG");
                out_lines->push_back("M=D");

                // LCL = RAM[endFrame - 4]
                out_lines->push_back("@4");
                out_lines->push_back("D=A");
                out_lines->push_back("@R13");
                out_lines->push_back("A=M-D");
                out_lines->push_back("D=M");
                out_lines->push_back("@LCL");
                out_lines->push_back("M=D");

                // jump to retAddr (R14)
                out_lines->push_back("@R14");
                out_lines->push_back("A=M");
                out_lines->push_back("0;JMP");

                return {};
            },
            [&] (const cmd_call& cmd) -> tl::expected<void, std::string> {
                std::string return_label = fmt::format("{}$ret.{}", cmd.name, counter++);

                // RAM[SP+0] <- return address
                out_lines->push_back(fmt::format("@{}", return_label));
                out_lines->push_back("D=M");
                out_lines->push_back("@SP");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");
                // SP++
                out_lines->push_back("@SP");
                out_lines->push_back("M=M+1");

                // RAM[SP+1] <- LCL
                out_lines->push_back("@LCL");
                out_lines->push_back("A=M");
                out_lines->push_back("D=M");
                out_lines->push_back("@SP");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");
                // SP++
                out_lines->push_back("@SP");
                out_lines->push_back("M=M+1");

                // RAM[SP+1] <- ARG
                out_lines->push_back("@ARG");
                out_lines->push_back("A=M");
                out_lines->push_back("D=M");
                out_lines->push_back("@SP");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");
                // SP++
                out_lines->push_back("@SP");
                out_lines->push_back("M=M+1");

                // RAM[SP+1] <- THIS
                out_lines->push_back("@THIS");
                out_lines->push_back("A=M");
                out_lines->push_back("D=M");
                out_lines->push_back("@SP");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");
                // SP++
                out_lines->push_back("@SP");
                out_lines->push_back("M=M+1");

                // RAM[SP+1] <- THAT
                out_lines->push_back("@THAT");
                out_lines->push_back("A=M");
                out_lines->push_back("D=M");
                out_lines->push_back("@SP");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");
                // SP++
                out_lines->push_back("@SP");
                out_lines->push_back("M=M+1");

                // RAM[ARG] = RAM[SP] - 5 - nArgs
                out_lines->push_back("@5");
                out_lines->push_back("D=A");
                out_lines->push_back(fmt::format("@{}", cmd.count));
                out_lines->push_back("D=D+A");
                out_lines->push_back("@SP");
                out_lines->push_back("A=M");
                out_lines->push_back("D=M-D");
                out_lines->push_back("@ARG");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");

                // RAM[LCL] = RAM[SP]
                out_lines->push_back("@SP");
                out_lines->push_back("A=M");
                out_lines->push_back("D=A");
                out_lines->push_back("@LCL");
                out_lines->push_back("A=M");
                out_lines->push_back("M=D");

                // jump to function
                out_lines->push_back(fmt::format("@{}", cmd.name));
                out_lines->push_back("0;JMP");

                // (return_label)
                out_lines->push_back(fmt::format("({})", return_label));

                return {};
            },
            [&] (auto&&) -> tl::expected<void, std::string> {
                return tl::unexpected(fmt::format("Not Implemented: {}", line));
            },
        }, instr);

        if (!res.has_value()) {
            return tl::unexpected(res.error());
        }

        out_lines->push_back("");
    }
    return {};
}
