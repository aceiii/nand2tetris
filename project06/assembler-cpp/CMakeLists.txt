cmake_minimum_required(VERSION 3.15)

project(assembler-cpp CXX)

set(EXE_NAME assembler-cpp)
set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCE_FILES src/*.cpp)

add_subdirectory(thirdparty/spdlog)

add_executable(${EXE_NAME} ${SOURCE_FILES})

target_link_libraries(${EXE_NAME} spdlog)
