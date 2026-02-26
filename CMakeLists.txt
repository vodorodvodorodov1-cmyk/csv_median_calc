cmake_minimum_required(VERSION 3.23)
project(csv_median_calculator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.13.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

find_package(Boost 1.74 REQUIRED COMPONENTS program_options)
find_package(Threads REQUIRED)

set(COMMON_INCLUDES src ${Boost_INCLUDE_DIRS})
set(COMMON_LIBS
    Boost::program_options
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    Threads::Threads
)
set(COMMON_FLAGS $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

# ── Основное приложение ────────────────────────────────────────────────────────
add_executable(csv_median_calculator src/main.cpp)
target_include_directories(csv_median_calculator PRIVATE ${COMMON_INCLUDES})
target_link_libraries(csv_median_calculator PRIVATE ${COMMON_LIBS})
target_compile_options(csv_median_calculator PRIVATE ${COMMON_FLAGS})

# ── Unit-тесты ─────────────────────────────────────────────────────────────────
enable_testing()

add_executable(unit_tests
    tests/unit/test_median_calculator.cpp
    tests/unit/test_stats_aggregator.cpp
    tests/unit/test_csv_reader.cpp
    tests/unit/test_config_parser.cpp
)
target_include_directories(unit_tests PRIVATE ${COMMON_INCLUDES})
target_link_libraries(unit_tests PRIVATE ${COMMON_LIBS} Catch2::Catch2WithMain)
target_compile_options(unit_tests PRIVATE ${COMMON_FLAGS})

include(CTest)
include(Catch)
catch_discover_tests(unit_tests)
