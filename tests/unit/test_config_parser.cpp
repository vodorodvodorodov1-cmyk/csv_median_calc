#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "config_parser.hpp"

namespace fs = std::filesystem;
using config::parse;
using config::metric_type;

namespace {

struct temp_toml {
    fs::path path;
    explicit temp_toml(std::string_view content_) {
        path = fs::temp_directory_path() /
               ("cfg_test_" + std::to_string(std::rand()) + ".toml");
        std::ofstream f(path);
        f << content_;
    }
    ~temp_toml() { fs::remove(path); }
};

// Директория гарантированно существует на любой Linux-системе
constexpr std::string_view k_existing_dir = "/tmp";

} // namespace

TEST_CASE("config: минимальный валидный конфиг") {
    temp_toml cfg(std::format("[main]\ninput = '{}'\n", k_existing_dir));
    const auto result = parse(cfg.path);
    REQUIRE(result.input_dir == k_existing_dir);
    REQUIRE(result.filename_masks.empty());
    REQUIRE(!result.output_dir.empty());
    // Медиана включена по умолчанию
    REQUIRE(result.metrics.size() == 1);
    REQUIRE(result.metrics[0] == metric_type::median);
}

TEST_CASE("config: отсутствие input бросает исключение") {
    temp_toml cfg("[main]\noutput = '/tmp'\n");
    REQUIRE_THROWS_AS(parse(cfg.path), std::runtime_error);
}

TEST_CASE("config: несуществующая input директория бросает исключение") {
    temp_toml cfg("[main]\ninput = '/this_dir_does_not_exist_xyz_abc'\n");
    REQUIRE_THROWS_AS(parse(cfg.path), std::runtime_error);
}

TEST_CASE("config: несуществующий файл бросает исключение") {
    REQUIRE_THROWS_AS(
        parse("/tmp/nonexistent_config_xyz.toml"), std::runtime_error);
}

TEST_CASE("config: filename_mask парсится корректно") {
    temp_toml cfg(std::format(
        "[main]\ninput = '{}'\nfilename_mask = ['level', 'trade']\n",
        k_existing_dir));
    const auto result = parse(cfg.path);
    REQUIRE(result.filename_masks.size() == 2);
    REQUIRE(result.filename_masks[0] == "level");
    REQUIRE(result.filename_masks[1] == "trade");
}

TEST_CASE("config: [metrics].enabled парсится корректно") {
    temp_toml cfg(std::format(
        "[main]\ninput = '{}'\n"
        "[metrics]\nenabled = ['median', 'mean', 'p90']\n",
        k_existing_dir));
    const auto result = parse(cfg.path);
    REQUIRE(result.metrics.size() == 3);
    REQUIRE(result.metrics[0] == metric_type::median);
    REQUIRE(result.metrics[1] == metric_type::mean);
    REQUIRE(result.metrics[2] == metric_type::p90);
}

TEST_CASE("config: неизвестная метрика бросает исключение") {
    temp_toml cfg(std::format(
        "[main]\ninput = '{}'\n"
        "[metrics]\nenabled = ['median', 'unknown_metric']\n",
        k_existing_dir));
    REQUIRE_THROWS_AS(parse(cfg.path), std::runtime_error);
}

TEST_CASE("config: [performance].parallel и streaming парсятся") {
    temp_toml cfg(std::format(
        "[main]\ninput = '{}'\n"
        "[performance]\nparallel = true\nstreaming = false\n",
        k_existing_dir));
    const auto result = parse(cfg.path);
    REQUIRE(result.performance.parallel  == true);
    REQUIRE(result.performance.streaming == false);
}

TEST_CASE("config: output директория по умолчанию не пустая") {
    temp_toml cfg(std::format("[main]\ninput = '{}'\n", k_existing_dir));
    const auto result = parse(cfg.path);
    REQUIRE(!result.output_dir.string().empty());
}
