/**
 * \file stats_aggregator.hpp
 * \brief Инкрементальные метрики через Boost.Accumulators
 */
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/extended_p_square.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>

namespace stats {

namespace ba = boost::accumulators;

/**
 * \brief Индексы квантилей в массиве результатов extended_p_square.
 * Порядок должен совпадать с k_quantile_probs.
 */
enum quantile_index : std::size_t { p50 = 0, p90 = 1, p95 = 2, p99 = 3 };

class stats_aggregator {
public:
    explicit stats_aggregator()
        : _acc(ba::tag::extended_p_square::probabilities = k_quantile_probs) {}

    void add(double value_) noexcept {
        _acc(value_);
        ++_count;
    }

    [[nodiscard]] double mean()    const noexcept { return ba::mean(_acc); }
    [[nodiscard]] double std_dev() const noexcept { return std::sqrt(ba::variance(_acc)); }

    /**
     * \param idx_  индекс квантиля из quantile_index (p50/p90/p95/p99)
     */
    [[nodiscard]] double percentile(std::size_t idx_) const noexcept {
        return ba::extended_p_square(_acc)[idx_];
    }

    [[nodiscard]] std::size_t count() const noexcept { return _count; }

private:
    using acc_t = ba::accumulator_set<double, ba::stats<
        ba::tag::mean,
        ba::tag::variance,
        ba::tag::extended_p_square
    >>;

    // Вероятности для Extended P-Square: p50, p90, p95, p99
    inline static const std::vector<double> k_quantile_probs{0.50, 0.90, 0.95, 0.99};

    acc_t       _acc;
    std::size_t _count{0};
};

} // namespace stats
