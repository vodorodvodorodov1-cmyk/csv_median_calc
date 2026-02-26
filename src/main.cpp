/**
 * \file main.cpp
 * \brief Точка входа: парсинг аргументов, конфигурации, основной цикл
 * \version 1.0.0
 */

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "config_parser.hpp"
#include "csv_reader.hpp"
#include "median_calculator.hpp"
#include "stats_aggregator.hpp"

namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace {

constexpr std::string_view k_version = "1.0.0";

[[nodiscard]] std::string fmt_price(double price_, int prec_ = 8) noexcept {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec_) << price_;
    return ss.str();
}

[[nodiscard]] fs::path default_config_path() noexcept {
    std::error_code ec;
    const auto exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() / "config.toml"
              : exe.parent_path() / "config.toml";
}

// ── Снимок значений всех включённых метрик в один момент времени ────────────
using snapshot = std::vector<double>;

[[nodiscard]] snapshot make_snapshot(
    const stats::median_calculator& median_,
    const stats::stats_aggregator&  stats_,
    const std::vector<config::metric_type>& metrics_)
{
    snapshot snap;
    snap.reserve(metrics_.size());
    for (const auto m : metrics_) {
        switch (m) {
            case config::metric_type::median:  snap.push_back(median_.median()); break;
            case config::metric_type::mean:    snap.push_back(stats_.mean());    break;
            case config::metric_type::std_dev: snap.push_back(stats_.std_dev()); break;
            case config::metric_type::p50: snap.push_back(stats_.percentile(stats::p50)); break;
            case config::metric_type::p90: snap.push_back(stats_.percentile(stats::p90)); break;
            case config::metric_type::p95: snap.push_back(stats_.percentile(stats::p95)); break;
            case config::metric_type::p99: snap.push_back(stats_.percentile(stats::p99)); break;
        }
    }
    return snap;
}

// ── Строит CSV-заголовок из включённых метрик ────────────────────────────────
[[nodiscard]] std::string build_header(
    const std::vector<config::metric_type>& metrics_)
{
    std::string h = "receive_ts";
    for (const auto m : metrics_) {
        h += ';';
        h += config::metric_to_column(m);
    }
    h += '\n';
    return h;
}

// ── Основной цикл обработки ──────────────────────────────────────────────────
struct processor {
    stats::median_calculator                 median_calc;
    stats::stats_aggregator                  stats_calc;
    const std::vector<config::metric_type>&  metrics;
    std::ofstream&                           out;
    std::optional<snapshot>                  prev_snap;
    std::size_t                              written_count{0};

    void operator()(const csv::trade_record& rec_) {
        median_calc.add(rec_.price);
        stats_calc.add(rec_.price);

        const auto cur = make_snapshot(median_calc, stats_calc, metrics);
        if (!prev_snap.has_value() || cur != *prev_snap) {
            out << rec_.receive_ts;
            for (const double v : cur) out << ';' << fmt_price(v);
            out << '\n';
            prev_snap = cur;
            ++written_count;
        }
    }
};

int run(const config::app_config& cfg_,
        const std::shared_ptr<spdlog::logger>& log_)
{
    std::error_code ec;
    fs::create_directories(cfg_.output_dir, ec);
    if (ec) {
        log_->error("Не удалось создать выходную директорию '{}': {}",
                    cfg_.output_dir.string(), ec.message());
        return 1;
    }
    log_->info("Входная директория:  {}", cfg_.input_dir.string());
    log_->info("Выходная директория: {}", cfg_.output_dir.string());

    const auto out_path = cfg_.output_dir / "median_result.csv";
    std::ofstream out_file(out_path);
    if (!out_file.is_open()) {
        log_->error("Не удалось открыть файл для записи: {}", out_path.string());
        return 1;
    }
    out_file << build_header(cfg_.metrics);

    processor proc{.metrics = cfg_.metrics, .out = out_file};

    if (cfg_.performance.streaming) {
        log_->info("Режим: потоковый k-way merge (7.3)");
        csv::stream_merge(cfg_.input_dir, cfg_.filename_masks, log_,
                          [&proc](const csv::trade_record& r_) { proc(r_); });

    } else if (cfg_.performance.parallel) {
        log_->info("Режим: параллельное чтение файлов (7.1)");
        const auto records = csv::load_parallel(
            cfg_.input_dir, cfg_.filename_masks, log_);
        log_->info("Прочитано записей: {}", records.size());
        for (const auto& r : records) proc(r);

    } else {
        const auto records = csv::load_and_merge(
            cfg_.input_dir, cfg_.filename_masks, log_);
        if (records.empty()) {
            log_->warn("Нет данных для обработки");
            return 0;
        }
        log_->info("Прочитано записей: {}", records.size());
        for (const auto& r : records) proc(r);
    }

    log_->info("Записано строк: {}", proc.written_count);
    log_->info("Результат сохранён: {}", out_path.string());
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    auto log = spdlog::stdout_color_mt("main");
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    log->info("Запуск csv_median_calculator v{}", k_version);

    po::options_description desc("Опции");
    desc.add_options()
        ("help,h",  "Показать справку")
        ("config",  po::value<std::string>(), "Путь к конфигурационному файлу")
        ("cfg",     po::value<std::string>(), "Псевдоним --config");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        log->error("Ошибка аргументов: {}", e.what());
        std::cerr << desc << '\n';
        return 1;
    }

    if (vm.count("help")) { std::cout << desc << '\n'; return 0; }

    fs::path config_path;
    if      (vm.count("config")) config_path = vm["config"].as<std::string>();
    else if (vm.count("cfg"))    config_path = vm["cfg"].as<std::string>();
    else {
        config_path = default_config_path();
        log->info("Конфиг не указан, используется: {}", config_path.string());
    }

    log->info("Чтение конфигурации: {}", config_path.string());

    config::app_config cfg;
    try {
        cfg = config::parse(config_path);
    } catch (const std::exception& e) {
        log->error("Ошибка конфигурации: {}", e.what());
        return 1;
    }

    const int result = run(cfg, log);
    log->info("Завершение работы");
    return result;
}
