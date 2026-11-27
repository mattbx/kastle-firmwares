# Desktop harness for our Kastle 2 firmwares' DSP.
# MIT License, Copyright (c) 2026 kastle-firmwares contributors.
#
#   cmake -S lab/harness -B lab/harness/build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build lab/harness/build
#   ctest --test-dir lab/harness/build --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(kastle_harness CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../code/src)

# The same strictness the firmware build would want, plus no exceptions/RTTI.
set(HARNESS_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)

add_executable(lasting_damage_tests tests.cpp)
target_include_directories(lasting_damage_tests PRIVATE ${SRC})
target_compile_options(lasting_damage_tests PRIVATE -Wall -Wextra)

add_executable(lasting_damage_render render.cpp)
target_include_directories(lasting_damage_render PRIVATE ${SRC})
target_compile_options(lasting_damage_render PRIVATE -Wall -Wextra)

# The DSP core alone, under the strict warnings and firmware-like flags.
add_library(lasting_damage_portability OBJECT portability.cpp)
target_include_directories(lasting_damage_portability PRIVATE ${SRC})
target_compile_options(lasting_damage_portability PRIVATE ${HARNESS_WARNINGS} -fno-exceptions -fno-rtti)

enable_testing()
add_test(NAME lasting_damage_tests COMMAND lasting_damage_tests)
