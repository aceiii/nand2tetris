#include <spdlog/spdlog.h>
#include <argparse/argparse.hpp>
#include <tl/expected.hpp>

#include <iostream>

auto main() -> int {
    spdlog::info("Hello, world!");
    return 0;
}
