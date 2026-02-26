#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>

#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "csv_reader.hpp"

namespace fs = std::filesystem;
using csv::split;
using csv::parse_file;

namespace {

auto make_null_logger() {
    static std::atomic<int> counter{0};
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>(
        "null_" + std::to_string(counter++), sink);
}

struct temp_csv {
    fs::path path;
    explicit temp_csv(std::string_view content_) {
        path = fs::temp_directory_path() /
               ("csv_test_" + std::to_string(std::rand()) + ".csv");
        std::ofstream f(path);
        f << content_;
    }
    ~temp_csv() { fs::remove(path); }
};

} // namespace

// ── split ─────────────────────────────────────────────────────────────────────

TEST_CASE("split: стандартный случай") {
    const auto p = split("a;b;c", ';');
    REQUIRE(p.size() == 3);
    REQUIRE(p[0] == "a");
    REQUIRE(p[1] == "b");
    REQUIRE(p[2] == "c");
}

TEST_CASE("split: один токен без разделителя") {
    const auto p = split("hello", ';');
    REQUIRE(p.size() == 1);
    REQUIRE(p[0] == "hello");
}

TEST_CASE("split: пустая строка даёт один пустой токен") {
    const auto p = split("", ';');
    REQUIRE(p.size() == 1);
    REQUIRE(p[0] == "");
}

TEST_CASE("split: строка из ТЗ (6 колонок)") {
    const auto p = split(
        "1716810808593627;1716810808574000;68480.00;10.109;bid;1", ';');
    REQUIRE(p.size() == 6);
    REQUIRE(p[2] == "68480.00");
}

// ── parse_file ────────────────────────────────────────────────────────────────

TEST_CASE("parse_file: корректный trade CSV") {
    auto log = make_null_logger();
    temp_csv f(
        "receive_ts;exchange_ts;price;quantity;side\n"
        "1000;900;100.0;1.0;bid\n"
        "2000;1900;200.0;2.0;ask\n");

    std::vector<csv::trade_record> records;
    REQUIRE_NOTHROW(parse_file(f.path, records, log));
    REQUIRE(records.size() == 2);
    REQUIRE(records[0].receive_ts == 1000);
    REQUIRE(records[0].price == 100.0);
    REQUIRE(records[1].receive_ts == 2000);
}

TEST_CASE("parse_file: корректный level CSV (с колонкой rebuild)") {
    auto log = make_null_logger();
    temp_csv f(
        "receive_ts;exchange_ts;price;quantity;side;rebuild\n"
        "5000;4900;50.0;3.0;bid;1\n");

    std::vector<csv::trade_record> records;
    REQUIRE_NOTHROW(parse_file(f.path, records, log));
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].price == 50.0);
}

TEST_CASE("parse_file: отсутствие колонки price бросает исключение") {
    auto log = make_null_logger();
    temp_csv f(
        "receive_ts;exchange_ts;quantity;side\n"
        "1000;900;1.0;bid\n");

    std::vector<csv::trade_record> records;
    REQUIRE_THROWS(parse_file(f.path, records, log));
}

TEST_CASE("parse_file: отсутствие колонки receive_ts бросает исключение") {
    auto log = make_null_logger();
    temp_csv f(
        "exchange_ts;price;quantity;side\n"
        "900;100.0;1.0;bid\n");

    std::vector<csv::trade_record> records;
    REQUIRE_THROWS(parse_file(f.path, records, log));
}

TEST_CASE("parse_file: пустой файл — нет записей, нет исключения") {
    auto log = make_null_logger();
    temp_csv f("");
    std::vector<csv::trade_record> records;
    REQUIRE_NOTHROW(parse_file(f.path, records, log));
    REQUIRE(records.empty());
}

TEST_CASE("parse_file: строка с некорректной ценой пропускается") {
    auto log = make_null_logger();
    temp_csv f(
        "receive_ts;exchange_ts;price;quantity;side\n"
        "1000;900;not_a_number;1.0;bid\n"
        "2000;1900;200.0;1.0;ask\n");

    std::vector<csv::trade_record> records;
    REQUIRE_NOTHROW(parse_file(f.path, records, log));
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].receive_ts == 2000);
}

TEST_CASE("parse_file: строка с некорректным ts пропускается") {
    auto log = make_null_logger();
    temp_csv f(
        "receive_ts;exchange_ts;price;quantity;side\n"
        "BAD_TS;900;100.0;1.0;bid\n"
        "2000;1900;200.0;1.0;ask\n");

    std::vector<csv::trade_record> records;
    REQUIRE_NOTHROW(parse_file(f.path, records, log));
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].receive_ts == 2000);
}

TEST_CASE("parse_file: строка с недостаточным числом колонок пропускается") {
    auto log = make_null_logger();
    temp_csv f(
        "receive_ts;exchange_ts;price;quantity;side\n"
        "1000;900\n"
        "2000;1900;300.0;1.0;ask\n");

    std::vector<csv::trade_record> records;
    REQUIRE_NOTHROW(parse_file(f.path, records, log));
    REQUIRE(records.size() == 1);
}
