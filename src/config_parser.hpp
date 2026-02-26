/**
 * \file config_parser.hpp
 * \brief Чтение и валидация конфигурационного файла TOML
 */
#pragma once

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

namespace config {

namespace fs = std::filesystem;

enum class metric_type { median, mean, std_dev, p50, p90, p95, p99 };

[[nodiscard]] inline std::optional<metric_type> metric_from_string(std::string_view s_) noexcept {
    if (s_ == "median")  return metric_type::median;
    if (s_ == "mean")    return metric_type::mean;
    if (s_ == "std_dev") return metric_type::std_dev;
    if (s_ == "p50")     return metric_type::p50;
    if (s_ == "p90")     return metric_type::p90;
    if (s_ == "p95")     return metric_type::p95;
    if (s_ == "p99")     return metric_type::p99;
    return std::nullopt;
}

[[nodiscard]] inline std::string_view metric_to_column(metric_type m_) noexcept {
    switch (m_) {
        case metric_type::median:  return "price_median";
        case metric_type::mean:    return "price_mean";
        case metric_type::std_dev: return "price_std_dev";
        case metric_type::p50:     return "price_p50";
        case metric_type::p90:     return "price_p90";
        case metric_type::p95:     return "price_p95";
        case metric_type::p99:     return "price_p99";
    }
    return "";
}

struct performance_config {
    bool parallel  = false;
    bool streaming = false;
};

struct app_config {
    fs::path                     input_dir;
    fs::path                     output_dir;
    std::vector<std::string>     filename_masks;
    std::vector<metric_type>     metrics{metric_type::median};
    performance_config           performance;
};

/**
 * \brief Читает и валидирует конфигурационный файл TOML.
 * \warning бросает std::runtime_error при любых проблемах
 */
[[nodiscard]] inline app_config parse(const fs::path& config_path_) {
    if (!fs::exists(config_path_)) {
        throw std::runtime_error(std::format(
            "Конфигурационный файл не найден: {}", config_path_.string()));
    }

    toml::table tbl;
    try {
        tbl = toml::parse_file(config_path_.string());
    } catch (const toml::parse_error& e) {
        throw std::runtime_error(std::format(
            "Ошибка парсинга TOML '{}': {}", config_path_.string(), e.what()));
    }

    app_config cfg;

    // ── [main].input ──────────────────────────────────────────────────────────
    const auto input_node = tbl["main"]["input"];
    if (!input_node) {
        throw std::runtime_error(
            "Обязательный параметр [main].input отсутствует");
    }
    const auto input_str = input_node.value<std::string>();
    if (!input_str) {
        throw std::runtime_error("[main].input должен быть строкой");
    }
    cfg.input_dir = *input_str;
    if (!fs::exists(cfg.input_dir) || !fs::is_directory(cfg.input_dir)) {
        throw std::runtime_error(std::format(
            "Входная директория не существует: {}", cfg.input_dir.string()));
    }

    // ── [main].output ─────────────────────────────────────────────────────────
    const auto output_node = tbl["main"]["output"];
    if (output_node) {
        const auto output_str = output_node.value<std::string>();
        if (!output_str) {
            throw std::runtime_error("[main].output должен быть строкой");
        }
        cfg.output_dir = *output_str;
    } else {
        cfg.output_dir = fs::current_path() / "output";
    }

    // ── [main].filename_mask ──────────────────────────────────────────────────
    if (const auto* arr = tbl["main"]["filename_mask"].as_array()) {
        for (const auto& item : *arr) {
            if (auto s = item.value<std::string>()) {
                cfg.filename_masks.push_back(*s);
            }
        }
    }

    // ── [metrics].enabled ─────────────────────────────────────────────────────
    if (const auto* arr = tbl["metrics"]["enabled"].as_array()) {
        cfg.metrics.clear();
        for (const auto& item : *arr) {
            if (auto s = item.value<std::string>()) {
                if (auto m = metric_from_string(*s)) {
                    cfg.metrics.push_back(*m);
                } else {
                    throw std::runtime_error(std::format(
                        "Неизвестная метрика: '{}'. "
                        "Допустимые: median, mean, std_dev, p50, p90, p95, p99", *s));
                }
            }
        }
        if (cfg.metrics.empty()) {
            throw std::runtime_error("[metrics].enabled не должен быть пустым");
        }
    }

    // ── [performance] ─────────────────────────────────────────────────────────
    if (auto v = tbl["performance"]["parallel"].value<bool>()) {
        cfg.performance.parallel = *v;
    }
    if (auto v = tbl["performance"]["streaming"].value<bool>()) {
        cfg.performance.streaming = *v;
    }

    return cfg;
}

} // namespace config
