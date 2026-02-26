#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "stats_aggregator.hpp"

using stats::stats_aggregator;
using stats::quantile_index;
using Catch::Matchers::WithinAbs;

TEST_CASE("stats_aggregator: count") {
    stats_aggregator agg;
    REQUIRE(agg.count() == 0);
    agg.add(1.0);
    REQUIRE(agg.count() == 1);
    agg.add(2.0);
    REQUIRE(agg.count() == 2);
}

TEST_CASE("stats_aggregator: mean равномерного набора") {
    stats_aggregator agg;
    for (double v : {1.0, 2.0, 3.0, 4.0, 5.0}) agg.add(v);
    REQUIRE_THAT(agg.mean(), WithinAbs(3.0, 1e-9));
}

TEST_CASE("stats_aggregator: mean одного элемента") {
    stats_aggregator agg;
    agg.add(42.0);
    REQUIRE_THAT(agg.mean(), WithinAbs(42.0, 1e-9));
}

TEST_CASE("stats_aggregator: std_dev константной последовательности равен нулю") {
    stats_aggregator agg;
    for (int i = 0; i < 5; ++i) agg.add(10.0);
    REQUIRE_THAT(agg.std_dev(), WithinAbs(0.0, 1e-9));
}

TEST_CASE("stats_aggregator: std_dev известного набора") {
    // {2,4,4,4,5,5,7,9}: mean=5, population variance=4, stddev=2
    stats_aggregator agg;
    for (double v : {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0}) agg.add(v);
    REQUIRE_THAT(agg.mean(),    WithinAbs(5.0, 1e-9));
    REQUIRE_THAT(agg.std_dev(), WithinAbs(2.0, 1e-9));
}

TEST_CASE("stats_aggregator: p50 аппроксимирует медиану (±5%)") {
    stats_aggregator agg;
    // 100 равномерно распределённых значений: истинный p50 = 50.5
    for (int i = 1; i <= 100; ++i) agg.add(static_cast<double>(i));
    REQUIRE_THAT(agg.percentile(quantile_index::p50), WithinAbs(50.5, 5.0));
}

TEST_CASE("stats_aggregator: p90 меньше p99 на монотонном наборе") {
    stats_aggregator agg;
    for (int i = 1; i <= 200; ++i) agg.add(static_cast<double>(i));
    // На монотонной последовательности p90 < p99 — базовая проверка порядка
    REQUIRE(agg.percentile(quantile_index::p90) < agg.percentile(quantile_index::p99));
}
