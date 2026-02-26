/**
 * \file csv_reader.hpp
 * \brief Чтение CSV: пакетный режим, параллельный , потоковый
 */
#pragma once

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace csv {

namespace fs = std::filesystem;

struct trade_record {
    std::int64_t receive_ts{};
    double       price{};
};

/**
 * \brief Разбивает строку по разделителю без копирования данных.
 * Возвращаемые view валидны пока жив source string.
 */
[[nodiscard]] inline std::vector<std::string_view> split(
    std::string_view line_,
    char delim_) noexcept
{
    std::vector<std::string_view> parts;
    parts.reserve(6);
    std::size_t start = 0;
    while (start <= line_.size()) {
        auto end = line_.find(delim_, start);
        if (end == std::string_view::npos) end = line_.size();
        parts.emplace_back(line_.data() + start, end - start);
        start = end + 1;
    }
    return parts;
}

template<typename Logger>
inline void parse_file(
    const fs::path&            file_path_,
    std::vector<trade_record>& out_,
    const Logger&              logger_)
{
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        throw std::runtime_error(std::format(
            "Не удалось открыть файл: {}", file_path_.string()));
    }

    std::string header_line;
    if (!std::getline(file, header_line)) {
        logger_->warn("Файл пуст: {}", file_path_.string());
        return;
    }
    if (!header_line.empty() && header_line.back() == '\r') header_line.pop_back();

    // Определяем индексы колонок динамически — работает с обоими форматами (level и trade)
    const auto header_cols = split(header_line, ';');
    int col_ts    = -1;
    int col_price = -1;
    for (int i = 0; i < static_cast<int>(header_cols.size()); ++i) {
        if (header_cols[i] == "receive_ts") col_ts    = i;
        if (header_cols[i] == "price")      col_price = i;
    }
    if (col_ts < 0) {
        throw std::runtime_error(std::format(
            "Колонка 'receive_ts' не найдена: {}", file_path_.string()));
    }
    if (col_price < 0) {
        throw std::runtime_error(std::format(
            "Колонка 'price' не найдена: {}", file_path_.string()));
    }

    const int required_cols = std::max(col_ts, col_price);
    std::string line;
    std::size_t line_num = 1;

    while (std::getline(file, line)) {
        ++line_num;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        const auto cols = split(line, ';');
        if (static_cast<int>(cols.size()) <= required_cols) {
            logger_->warn("{}:{} — недостаточно колонок, пропущена",
                          file_path_.string(), line_num);
            continue;
        }

        trade_record rec;
        const auto ts_sv = cols[col_ts];
        const auto [ptr, ec] = std::from_chars(
            ts_sv.data(), ts_sv.data() + ts_sv.size(), rec.receive_ts);
        if (ec != std::errc{}) {
            logger_->warn("{}:{} — некорректный receive_ts '{}', пропущена",
                          file_path_.string(), line_num, ts_sv);
            continue;
        }

        try {
            std::size_t n{};
            rec.price = std::stod(std::string(cols[col_price]), &n);
            if (n == 0) throw std::invalid_argument{""};
        } catch (...) {
            logger_->warn("{}:{} — некорректная цена '{}', пропущена",
                          file_path_.string(), line_num, cols[col_price]);
            continue;
        }

        out_.push_back(rec);
    }
}

[[nodiscard]] inline std::vector<fs::path> find_csv_files(
    const fs::path&                  dir_,
    const std::vector<std::string>&  masks_)
{
    std::vector<fs::path> result;
    for (const auto& entry : fs::directory_iterator(dir_)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".csv") continue;
        const std::string stem = entry.path().stem().string();
        const bool match = masks_.empty() ||
            std::ranges::any_of(masks_, [&stem](const std::string& m_) {
                return stem.find(m_) != std::string::npos;
            });
        if (match) result.push_back(entry.path());
    }
    return result;
}

// ── Пакетный режим ─────────────────────────────────────────────────────────────
template<typename Logger>
[[nodiscard]] inline std::vector<trade_record> load_and_merge(
    const fs::path&                  dir_,
    const std::vector<std::string>&  masks_,
    const Logger&                    logger_)
{
    const auto files = find_csv_files(dir_, masks_);
    if (files.empty()) {
        logger_->warn("Подходящих CSV-файлов не найдено в '{}'", dir_.string());
        return {};
    }
    logger_->info("Найдено файлов: {}", files.size());
    for (const auto& p : files) logger_->info("  - {}", p.filename().string());

    std::vector<trade_record> all;
    for (const auto& p : files) {
        const std::size_t before = all.size();
        parse_file(p, all, logger_);
        logger_->info("  {} → {} записей", p.filename().string(), all.size() - before);
    }
    std::ranges::stable_sort(all, {}, &trade_record::receive_ts);
    return all;
}

// Параллельное чтение файлов ───────────────────────────────────────────


template<typename Logger>
[[nodiscard]] inline std::vector<trade_record> load_parallel(
    const fs::path&                  dir_,
    const std::vector<std::string>&  masks_,
    const Logger&                    logger_)
{
    const auto files = find_csv_files(dir_, masks_);
    if (files.empty()) {
        logger_->warn("Подходящих CSV-файлов не найдено в '{}'", dir_.string());
        return {};
    }
    logger_->info("Параллельное чтение {} файлов", files.size());

    std::vector<std::future<std::vector<trade_record>>> futures;
    futures.reserve(files.size());

    for (const auto& path : files) {
        futures.push_back(std::async(std::launch::async,
            [&path, &logger_]() -> std::vector<trade_record> {
                std::vector<trade_record> records;
                parse_file(path, records, logger_);
                return records;
            }));
    }

    std::vector<trade_record> all;
    for (std::size_t i = 0; i < futures.size(); ++i) {
        auto records = futures[i].get();
        logger_->info("  {} → {} записей",
                      files[i].filename().string(), records.size());
        all.insert(all.end(),
                   std::make_move_iterator(records.begin()),
                   std::make_move_iterator(records.end()));
    }
    std::ranges::stable_sort(all, {}, &trade_record::receive_ts);
    return all;
}

//Потоковая обработка (k-way merge) ────────────────────────────────────

namespace detail {

struct file_cursor {
    std::ifstream stream;
    std::string   buf;
    trade_record  current;
    int           col_ts    = -1;
    int           col_price = -1;
    bool          exhausted = false;

    file_cursor(const file_cursor&)            = delete;
    file_cursor& operator=(const file_cursor&) = delete;
    file_cursor(file_cursor&&)                 = default;
};

template<typename Logger>
inline bool advance(file_cursor& cur_, const Logger& logger_) {
    while (std::getline(cur_.stream, cur_.buf)) {
        if (!cur_.buf.empty() && cur_.buf.back() == '\r') cur_.buf.pop_back();
        if (cur_.buf.empty()) continue;

        const auto cols = split(cur_.buf, ';');
        const int  req  = std::max(cur_.col_ts, cur_.col_price);
        if (static_cast<int>(cols.size()) <= req) continue;

        const auto ts_sv = cols[cur_.col_ts];
        if (auto [p, ec] = std::from_chars(
                ts_sv.data(), ts_sv.data() + ts_sv.size(),
                cur_.current.receive_ts); ec != std::errc{}) continue;

        try {
            std::size_t n{};
            cur_.current.price = std::stod(std::string(cols[cur_.col_price]), &n);
            if (n == 0) continue;
        } catch (...) { continue; }

        return true;
    }
    cur_.exhausted = true;
    return false;
}

template<typename Logger>
inline std::unique_ptr<file_cursor> open_cursor(
    const fs::path& path_,
    const Logger&   logger_)
{
    auto cur = std::make_unique<file_cursor>();
    cur->stream.open(path_);
    if (!cur->stream.is_open()) {
        logger_->error("Не удалось открыть: {}", path_.string());
        return nullptr;
    }

    std::string header;
    if (!std::getline(cur->stream, header)) {
        logger_->warn("Файл пуст: {}", path_.string());
        return nullptr;
    }
    if (!header.empty() && header.back() == '\r') header.pop_back();

    const auto cols = split(header, ';');
    for (int i = 0; i < static_cast<int>(cols.size()); ++i) {
        if (cols[i] == "receive_ts") cur->col_ts    = i;
        if (cols[i] == "price")      cur->col_price = i;
    }
    if (cur->col_ts < 0 || cur->col_price < 0) {
        logger_->error("Отсутствуют обязательные колонки: {}", path_.string());
        return nullptr;
    }

    if (!advance(*cur, logger_)) return nullptr;
    return cur;
}

} // namespace detail

template<typename Callback, typename Logger>
inline void stream_merge(
    const fs::path&                  dir_,
    const std::vector<std::string>&  masks_,
    const Logger&                    logger_,
    Callback&&                       on_record_)
{
    const auto files = find_csv_files(dir_, masks_);
    if (files.empty()) {
        logger_->warn("Подходящих CSV-файлов не найдено в '{}'", dir_.string());
        return;
    }
    logger_->info("Потоковый режим: {} файлов", files.size());

    std::vector<std::unique_ptr<detail::file_cursor>> cursors;
    cursors.reserve(files.size());
    for (const auto& p : files) {
        auto cur = detail::open_cursor(p, logger_);
        if (cur) cursors.push_back(std::move(cur));
    }

    // Min-heap по receive_ts: raw-указатели валидны пока cursors жив
    auto cmp = [](const detail::file_cursor* a_, const detail::file_cursor* b_) {
        return a_->current.receive_ts > b_->current.receive_ts;
    };
    std::priority_queue<
        detail::file_cursor*,
        std::vector<detail::file_cursor*>,
        decltype(cmp)> pq(cmp);

    for (auto& c : cursors) pq.push(c.get());

    while (!pq.empty()) {
        auto* cur = pq.top();
        pq.pop();
        on_record_(cur->current);
        if (detail::advance(*cur, logger_)) pq.push(cur);
    }
}

} // namespace csv
