#pragma once

const char* const kDefaultBootstrapCode =
    "@256\n" // SP = 256
    "D=A\n"
    "@SP\n"
    "M=D\n"

    // RAM[SP+0] <- return address
    "D=0\n"
    "@SP\n"
    "A=M\n"
    "M=D\n"
    // SP++
    "@SP\n"
    "M=M+1\n"

    // RAM[SP+1] <- LCL
    "@LCL\n"
    "A=M\n"
    "D=M\n"
    "@SP\n"
    "A=M\n"
    "M=D\n"
    // SP++
    "@SP\n"
    "M=M+1\n"

    // RAM[SP+1] <- ARG
    "@ARG\n"
    "A=M\n"
    "D=M\n"
    "@SP\n"
    "A=M\n"
    "M=D\n"
    // SP++
    "@SP\n"
    "M=M+1\n"

    // RAM[SP+1] <- THIS
    "@THIS\n"
    "A=M\n"
    "D=M\n"
    "@SP\n"
    "A=M\n"
    "M=D\n"
    // SP++
    "@SP\n"
    "M=M+1\n"

    // RAM[SP+1] <- THAT
    "@THAT\n"
    "A=M\n"
    "D=M\n"
    "@SP\n"
    "A=M\n"
    "M=D\n"
    // SP++
    "@SP\n"
    "M=M+1\n"

    // RAM[ARG] = RAM[SP] - 5 - nArgs
    "@5\n"
    "D=A\n"
    "@SP\n"
    "A=M\n"
    "D=M-D\n"
    "@ARG\n"
    "A=M\n"
    "M=D\n"

    // RAM[LCL] = RAM[SP]
    "@SP\n"
    "A=M\n"
    "D=A\n"
    "@LCL\n"
    "A=M\n"
    "M=D\n"

    "@Sys.init\n" // call Sys.init
    "0;JMP\n";
